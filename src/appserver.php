<?php

/**
 * appserver.php
 *
 * NOTICE OF LICENSE
 *
 * This source file is subject to the Open Software License (OSL 3.0)
 * that is available through the world-wide-web at this URL:
 * http://opensource.org/licenses/osl-3.0.php
 */

/**
 * A php test script show functionality of the extension
 *
 * @copyright  	Copyright (c) 2013 <info@techdivision.com> - TechDivision GmbH
 * @license    	http://opensource.org/licenses/osl-3.0.php
 *              Open Software License (OSL 3.0)
 * @author      Johann Zelger <jz@techdivision.com>
 */

appserver_redefine('PHP_SAPI', 'appserver');
echo "CALL appserver_redefine('PHP_SAPI', 'appserver'): -> SAPI_TYPE: " . PHP_SAPI;
echo PHP_EOL . "==========================================". PHP_EOL;

appserver_set_headers_sent(true);
$result = var_export(headers_sent(), true);
echo "CALL appserver_set_headers_sent(true): -> headers_sent = " . $result . PHP_EOL;
appserver_set_headers_sent();
$result = var_export(headers_sent(), true);
echo "CALL appserver_set_headers_sent(false): -> headers_sent = " . $result;
echo PHP_EOL . "==========================================". PHP_EOL;

appserver_set_headers_sent(false);
header('X-Powered-By: appserver');
header('Location: http://www.appserver.io');
echo "CALL #1 appserver_get_headers(): -> ";
echo var_export(appserver_get_headers(), true);
echo PHP_EOL . "==========================================". PHP_EOL;
echo "CALL #2 appserver_get_headers(true): -> ";
echo var_export(appserver_get_headers(true), true);
echo PHP_EOL . "==========================================". PHP_EOL;
echo "CALL #3 appserver_get_headers(): -> ";
echo var_export(appserver_get_headers(), true);
echo PHP_EOL . "==========================================". PHP_EOL;


echo "CALL headers_sent(): -> ";
echo var_export(headers_sent(), true);
echo PHP_EOL . "==========================================". PHP_EOL;


$temp_filenames[] = tempnam('/tmp', '');
$temp_filenames[] = tempnam('/tmp', '');
$temp_filenames[] = tempnam('/tmp', '');
$temp_filenames[] = tempnam('/tmp', '');

foreach ($temp_filenames as $index => $temp_filename) {
    file_put_contents($temp_filename, 'Hi there iam a upload file.');
    echo "CALL #$index appserver_register_file_upload(): -> ";
    echo var_export(appserver_register_file_upload($temp_filename), true);
    echo PHP_EOL . "==========================================". PHP_EOL;

    echo "CALL #$index move_uploaded_file: -> " .var_export(move_uploaded_file($temp_filename, '/tmp/move_uploaded_file_success'), true);
    echo PHP_EOL . "==========================================". PHP_EOL;
}
