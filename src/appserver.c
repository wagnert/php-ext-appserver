/**
 * appserver.c
 *
 * NOTICE OF LICENSE
 *
 * This source file is subject to the Open Software License (OSL 3.0)
 * that is available through the world-wide-web at this URL:
 * http://opensource.org/licenses/osl-3.0.php
 */

/**
 * A php extension for the appserver project (http://appserver.io)
 *
 * It provides various functionality for usage within the appserver runtime.
 *
 * @copyright  	Copyright (c) 2013 <info@techdivision.com> - TechDivision GmbH
 * @license    	http://opensource.org/licenses/osl-3.0.php
 *              Open Software License (OSL 3.0)
 * @author      Johann Zelger <jz@techdivision.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_appserver.h"
#include "TSRM.h"
#include "SAPI.h"

#include "zend_API.h"
#include "zend_hash.h"
#include "zend_alloc.h"
#include "zend_operators.h"
#include "zend_globals.h"
#include "zend_compile.h"
#include "zend_constants.h"
#include "zend_closures.h"
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

ZEND_DECLARE_MODULE_GLOBALS(appserver)

static int le_appserver;

int (*appserver_orig_header_handler)(sapi_header_struct *h AS_SAPI_HEADER_OP_DC, sapi_headers_struct *s TSRMLS_DC);

const zend_function_entry appserver_functions[] = {
    PHP_FE(appserver_get_headers, NULL)
    PHP_FE(appserver_register_file_upload, NULL)
    PHP_FE(appserver_set_headers_sent, NULL)
    PHP_FE(appserver_redefine, NULL)
    PHP_FE_END
};

zend_module_entry appserver_module_entry = {
    STANDARD_MODULE_HEADER,
    APPSERVER_NAME,
    appserver_functions,
    PHP_MINIT(appserver),
    PHP_MSHUTDOWN(appserver),
    PHP_RINIT(appserver),
    PHP_RSHUTDOWN(appserver),
    PHP_MINFO(appserver),
    APPSERVER_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_APPSERVER
ZEND_GET_MODULE(appserver)
#endif


static void appserver_llist_string_destor(void *data)
{
    char *s = data;

    if (s) {
        free(s);
    }
}

static void appserver_header_remove_by_prefix(appserver_llist *headers, char *prefix, int prefix_len TSRMLS_DC)
{
    appserver_llist_item *item;
    char                 *header;

    // iterate through items
    for (item = APPSERVER_GLOBALS(headers)->head; item != NULL; item = item->next) {
    	// get value from item
        header = item->ptr;
        // check if prefix was found
        if ((strlen(header) > prefix_len + 1) && (header[prefix_len] == ':') && (strncasecmp(header, prefix, prefix_len) == 0)) {
            appserver_llist_item *current = item;
            // delete header
            appserver_llist_del(headers, current);
        }
    }
}

static int appserver_header_handler(sapi_header_struct *h AS_SAPI_HEADER_OP_DC, sapi_headers_struct *s TSRMLS_DC)
{

    if (APPSERVER_GLOBALS(headers)) {
        switch (op) {

            case SAPI_HEADER_ADD:
            	appserver_llist_add(APPSERVER_GLOBALS(headers), NULL, strdup(h->header));
                break;

            case SAPI_HEADER_REPLACE: {
                char *colon_offset = strchr(h->header, ':');

                if (colon_offset) {
                    char save = *colon_offset;

                    *colon_offset = '\0';
                    appserver_header_remove_by_prefix(APPSERVER_GLOBALS(headers), h->header, strlen(h->header) TSRMLS_CC);
                    *colon_offset = save;
                }

                appserver_llist_add(APPSERVER_GLOBALS(headers), NULL, strdup(h->header));
                }
                break;

            case SAPI_HEADER_DELETE_ALL:
                appserver_llist_clear(APPSERVER_GLOBALS(headers));
                break;

            case SAPI_HEADER_DELETE:
                break;

            case SAPI_HEADER_SET_STATUS:
                break;

        }
    }

    // Disables the orig header handler to avoid headers_sent check.

    // if (appserver_orig_header_handler) {
    //    return appserver_orig_header_handler(h AS_SAPI_HEADER_OP_CC, s TSRMLS_CC);
    // }

    // set headers not be sent yet
    SG(headers_sent) = 0;

    return SAPI_HEADER_ADD;
}

static void php_appserver_shutdown_globals (zend_appserver_globals *appserver_globals TSRMLS_DC)
{

}

static void php_appserver_init_globals(zend_appserver_globals *appserver_globals)
{
    /* Override header generation in SAPI */
    if (sapi_module.header_handler != appserver_header_handler) {
        appserver_orig_header_handler = sapi_module.header_handler;
        sapi_module.header_handler = appserver_header_handler;
    }

    appserver_globals->headers = NULL;
}

static inline void php_appserver_free_redefined(zval **pzval) 
{
    TSRMLS_FETCH();
    zval_dtor(*pzval);
    efree(*pzval);
}

PHP_MSHUTDOWN_FUNCTION(appserver)
{
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */

#ifdef ZTS
    ts_free_id(appserver_globals_id);
#else
    php_appserver_shutdown_globals(&appserver_globals TSRMLS_CC);
#endif

    return SUCCESS;
}

PHP_MINIT_FUNCTION(appserver)
{
    /* If you have INI entries, uncomment these lines
    REGISTER_INI_ENTRIES();
    */
    ZEND_INIT_MODULE_GLOBALS(appserver, php_appserver_init_globals, NULL);

    return SUCCESS;
}

PHP_RINIT_FUNCTION(appserver)
{
    APPSERVER_GLOBALS(headers) = appserver_llist_allocate(appserver_llist_string_destor);
    zend_hash_init(
        &APPSERVER_GLOBALS(redefined), 16, NULL, (dtor_func_t) php_appserver_free_redefined, 0);
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(appserver)
{
    zend_hash_destroy(
        &APPSERVER_GLOBALS(redefined));
    return SUCCESS;
}

PHP_MINFO_FUNCTION(appserver)
{
	php_info_print_table_start();
    php_info_print_table_header(2, "appserver support", "enabled");
    php_info_print_table_row(2, "Version", APPSERVER_VERSION);
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}

/* {{{ proto boolean appserver_redefine(string $constant [, mixed $value]) 
        redefine/undefine constant at runtime ... /* }}} */  
PHP_FUNCTION(appserver_redefine)
{
    char *name;
    zend_uint name_len;
    zval *pzval = NULL;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &name, &name_len, &pzval) == FAILURE) {
        return;
    }
    
    {
        zend_constant *defined = NULL;
        
        if (zend_hash_find(EG(zend_constants), name, name_len+1, (void**)&defined) == FAILURE) {
            char *lname = zend_str_tolower_dup(name, name_len);
            
            if (zend_hash_find(
                EG(zend_constants), lname, name_len+1, (void**)&defined) == SUCCESS) {
                if (defined->flags & CONST_CS)
                    defined = NULL;
            }
            
            efree(lname);
        }
        
        if (defined != NULL) {
            /* change user constant */
            if ((defined->module_number == PHP_USER_CONSTANT)) {
                zend_constant container = *defined;
                
                if (pzval)
                    container.name = zend_strndup(container.name, container.name_len);
                
                if (zend_hash_del(EG(zend_constants), name, name_len+1)==SUCCESS) {
                    if (pzval) {
                        SEPARATE_ZVAL(&pzval);
                        container.value = *pzval;
                        zend_hash_next_index_insert(
                            &APPSERVER_GLOBALS(redefined), &pzval, sizeof(zval**), NULL);
                        Z_ADDREF_P(pzval);
                        zend_register_constant(&container TSRMLS_CC);
                    }       
                }
            } else {
                /* change internal constant */
                SEPARATE_ZVAL(&pzval);
                defined->value = *pzval;
                zend_hash_next_index_insert(
                    &APPSERVER_GLOBALS(redefined), &pzval, sizeof(zval**), NULL);
                Z_ADDREF_P(pzval);
            }
        }
        
        RETURN_FALSE;
    }
}

PHP_FUNCTION(appserver_register_file_upload)
{
	char *path;
	int path_len;
	HashTable *uploaded_files = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE) {
		return;
	}

	// check if uploaded files sapi globals are not initialised
	if (!SG(rfc1867_uploaded_files)) {
		ALLOC_HASHTABLE(uploaded_files);
		zend_hash_init(uploaded_files, 5, NULL, NULL, 0);
		SG(rfc1867_uploaded_files) = uploaded_files;
	}

	// add to zend hash table
	zend_hash_add(SG(rfc1867_uploaded_files), path, path_len + 1, &path, sizeof(char *), NULL);

	RETURN_TRUE;
}

/* {{{ proto array appserver_get_headers(boolean $reset_headers_flag)
        gets headers set via header function and reset if flag was set ... /* }}} */
PHP_FUNCTION(appserver_get_headers)
{
    appserver_llist_item *header_item;
	zend_bool headers_reset_flag = 0;
    char                 *string;
    // parse params
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &headers_reset_flag) == FAILURE) {
        return;
    }
    // init return value as array
    array_init(return_value);
    for (header_item = APPSERVER_GLOBALS(headers)->head; header_item != NULL; header_item = header_item->next) {
        string = header_item->ptr;
        add_next_index_string(return_value, string, 1);
    }
    // clear headers globals hash if delete flag was given
    if (headers_reset_flag != 0) {
    	appserver_llist_clear(APPSERVER_GLOBALS(headers));
    }
}

PHP_FUNCTION(appserver_set_headers_sent)
{
	zend_bool headers_sent_flag = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &headers_sent_flag) == FAILURE) {
		return;
	}

	// set headers not be sent yet
	SG(headers_sent) = headers_sent_flag;
}


// ---------------------------------------------------------------------------
// Zend Extension Functions
// ---------------------------------------------------------------------------
ZEND_DLEXPORT void onStatement(zend_op_array *op_array)
{
    TSRMLS_FETCH();
}

int appserver_zend_startup(zend_extension *extension)
{
    TSRMLS_FETCH();
    CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;
    return zend_startup_module(&appserver_module_entry);
}

ZEND_DLEXPORT void appserver_zend_shutdown(zend_extension *extension)
{

}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API    ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
    "Application Server (appserver)",
    APPSERVER_VERSION,
    "TechDivision GmbH",
    "http://appserver.io/",
    "",
    appserver_zend_startup,
    appserver_zend_shutdown,
    NULL,              // activate_func_t
    NULL,               // deactivate_func_t
    NULL,               // message_handler_func_t
    NULL,               // op_array_handler_func_t
    onStatement,     // statement_handler_func_t
    NULL,           // fcall_begin_handler_func_t
    NULL,           // fcall_end_handler_func_t
    NULL,               // op_array_ctor_func_t
    NULL,               // op_array_dtor_func_t
    NULL,               // api_no_check
    COMPAT_ZEND_EXTENSION_PROPERTIES
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
