<?php
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST');

    function getURL() {
        $spaceReplacement = ' ';
        $toReplaceSpace = '+';
        
        $loc = "http://www.zillow.com/webservice/GetDeepSearchResults.htm?zws-id=X1-ZWz1dxi7b3wikr_5xiff&address=";

        $address = $_GET["streetInput"];
        $address = str_replace($spaceReplacement, $toReplaceSpace, $address);
        $loc .= $address;

        $loc .= "&citystatezip=";

        $city = $_GET["cityInput"];
        $city = str_replace($spaceReplacement, $toReplaceSpace, $city);
        $loc .= $city;
        
        $loc .= "%2C+";
        
        $state = $_GET["stateInput"];
        $loc .= $state;
        
        $loc .= "&rentzestimate=true";

        return $loc;
    }

    function getXMLFile($url) {
        $xmlFile = simplexml_load_file($url);
        $resultPath = 'response/results/result';

        if(count($xmlFile->xpath($resultPath)) == 0) {
            return false;
        }
        
        return $xmlFile;
    }
    
    function returnData() {
        $json = getURL();
        $arr = ['input' => $json];
        echo $arr['input'];
    }
    
    function createTable($xmlFile) {
        $na = "N/A";
        $default = "DEFAULT";
        $dataArr['hasResults'] = 'true';
        $zpid = $xmlFile->xpath('response/results/result/zpid')[0];
        
        $searchAddr = $xmlFile->xpath('request/address')[0];
        $searchCSZ = $xmlFile->xpath('request/citystatezip')[0];
        $searchRequest = ucwords($searchAddr . ' ' . $searchCSZ);
        $detailsLink = $xmlFile->xpath('response/results/result/links/homedetails')[0];
        $address = $xmlFile->xpath('response/results/result/address/zipcode')[0];
        $dataArr["homeLink"] = $detailsLink . "";
        $dataArr["homeName"] = $searchRequest . ", " . $address;
        
        //1 - get data for first row of property information table
        $propertyType = checkExists($xmlFile, "useCode", false, false, false);
        $lastSoldPrice = checkExists($xmlFile, "lastSoldPrice", false, true, false);
        $dataArr["propertyType"] = $propertyType . "";
        $dataArr["lastSoldPrice"] = $lastSoldPrice . "";

        //2
        $yearBuilt = checkExists($xmlFile, "yearBuilt", false, false, false);
        $lastSoldDate = checkExists($xmlFile, "lastSoldDate", false, false, true);
        $dataArr["yearBuilt"] = $yearBuilt . "";
        $dataArr["lastSoldDate"] = $lastSoldDate . "";

        //3
        $lotSize = checkExists($xmlFile, "lotSizeSqFt", false, false, false);
        if($lotSize != $na) {
            $lotSize .= " sq. ft. ";
        }
        $zestimateDate = checkExists($xmlFile, "zestimate/last-updated", false, false, true);
        $zestimate = checkExists($xmlFile, "zestimate/amount", false, true, false);
        $dataArr["lotSize"] = $lotSize . "";
        $dataArr["zestimateDate"] = $zestimateDate . "";
        $dataArr["zestimate"] = $zestimate . "";
        
        //4
        $finishedArea = checkExists($xmlFile, "finishedSqFt", false, false, false);
        if($finishedArea != $na) {
            $finishedArea .= " sq. ft. ";
        }
        $monthValueChange = checkExists($xmlFile, "zestimate/valueChange", true, true, false);
        if($monthValueChange != $na) {
            $arrowChoice = getArrowHTML($xmlFile->xpath('response/results/result/zestimate/valueChange')[0]);
        } else {
            $arrowChoice = $na;
        }
        $dataArr["monthValArrow"] = $arrowChoice . "";
        $dataArr["30DaysChange"] = $monthValueChange . "";
        $dataArr["finishedArea"] = $finishedArea . "";

        //5
        $bathrooms = checkExists($xmlFile, "bathrooms", false, false, false);
        $rangeLow = checkExists($xmlFile, "zestimate/valuationRange/low", false, true, false);
        $rangeHigh = checkExists($xmlFile, "zestimate/valuationRange/high", false, true, false);
        $range = $rangeLow . " - " . $rangeHigh;
        $dataArr["bathrooms"] = $bathrooms . "";
        $dataArr["zestimateValRange"] = $range . "";

        //6
        $bedrooms = checkExists($xmlFile, "bedrooms", false, false, false);
        $rentZestimateDate = checkExists($xmlFile, "rentzestimate/last-updated", false, false, true);
        $rentZestimate = checkExists($xmlFile, "rentzestimate/amount", false, true, false);
        $dataArr["bedrooms"] = $bedrooms . "";
        $dataArr["rentZestimateDate"] = $rentZestimateDate . "";
        $dataArr["rentZestimate"] = $rentZestimate . "";

        //7
        $taxAssessmentYear = checkExists($xmlFile, "taxAssessmentYear", false, false, false);
        $monthRentChange = checkExists($xmlFile, "rentzestimate/valueChange", true, true, false);
        if($monthRentChange != $na) {
            $arrowChoice = getArrowHTML($xmlFile->xpath('response/results/result/rentzestimate/valueChange')[0]);
        } else {
            $arrowChoice = $na;
        }
        $dataArr["taxYear"] = $taxAssessmentYear . "";
        $dataArr["monthRentChange"] = $monthRentChange . "";
        $dataArr["monthRentArrow"] = $arrowChoice . "";

        //8
        $taxAssessment = checkExists($xmlFile, "taxAssessment", false, true, false);
        $rentRangeLow = checkExists($xmlFile, "rentzestimate/valuationRange/low", false, true, false);
        $rentRangeHigh = checkExists($xmlFile, "rentzestimate/valuationRange/high", false, true, false);
        $rentRange = $rentRangeLow . " - " . $rentRangeHigh;
        $dataArr["taxAssessment"] = $taxAssessment . "";
        $dataArr["rentRange"] = $rentRange . "";
        
        $dataArr += getPictureURLs($zpid);
        
        return $dataArr;
    }

    function addComma($number) {
        $number = floatval($number);
        $number = number_format($number, $decimals = 2, $dec_point = ".", $thousands_sep = ",");
        $ret = "$" . $number;
        return $ret;
    }

    function formatDate($old) {
        $defaultUnknown = "01-Jan-1970";
        
        $monthArr = array("01" => "Jan", "02" => "Feb", "03" => "Mar", "04" => "Apr", "05" => "May", "06" => "Jun", "07" => "Jul", "08" => "Aug", "09" => "Sep", "10" => "Oct", "11" => "Nov", "12" => "Dec");
        $monthNum = substr($old, 0, 2);
        $month = $monthArr[$monthNum];
        $day = substr($old, 3, 2);
        $year = substr($old, 6, strlen($old)-1);
        $date =  ($day . "-" . $month . "-" . $year);
        
        return $date;
    }

    function getArrowHTML($val) {
        //$start = "<img src='imgs/";
        $arrow = "";
        if(substr($val, 0, 1) == "-") {
            $arrow .= "down_r.gif";
        } else {
            $arrow .= "up_g.gif";
        }
        
        return ($arrow);
    }

    function getAbsoluteValDollar($str) {
        if(substr($str, 0, 1) == "-") {
            $str = substr($str, 1, strlen($str)-1);
        }
        return $str;
    }
    
    function checkExists($xmlFile, $name, $getAbsoluteVal, $addComma, $formatDate) { 
        $default = "DEFAULT";
        $path = "response/results/result/";
        $path .= $name;

        if(count($xmlFile->xpath($path)) == 0) {
            return "N/A";
        } else {
            $toReturn = $xmlFile->xpath($path)[0];

            if($getAbsoluteVal) {
                $toReturn = getAbsoluteValDollar($toReturn);
            } 
            if($addComma) {
                $toReturn = addComma($toReturn);
            }
            if($formatDate) {
                $toReturn = formatDate($toReturn);
            }
        }
        return $toReturn;
    }

    function getPictureURLs($zpid) {
        $arrSize = 4; //number of pics in array
        $urlArr[0] = "http://www.zillow.com/webservice/GetChart.htm?zws-id=X1-ZWz1dxi7b3wikr_5xiff&unit-type=percent&width=600&height=300&chartDuration=1year&zpid=" . $zpid;
        $urlArr[1] = "http://www.zillow.com/webservice/GetChart.htm?zws-id=X1-ZWz1dxi7b3wikr_5xiff&unit-type=percent&width=600&height=300&chartDuration=5years&zpid=" . $zpid;
        $urlArr[2] = "http://www.zillow.com/webservice/GetChart.htm?zws-id=X1-ZWz1dxi7b3wikr_5xiff&unit-type=percent&width=600&height=300&chartDuration=10years&zpid=" . $zpid;
        $urlArr[3] = "http://www.zillow.com/webservice/GetChart.htm?zws-id=X1-ZWz1dxi7b3wikr_5xiff&unit-type=percent&width=300&height=200&zpid=" . $zpid;
        
        $picArrNames = [0 => '1year', 1 => '5year', 2 => '10year', 3 => 'small'];
        $picArr = ['1year' => 'empty', '5year' => 'empty', '10year' => 'empty', 'small' => 'empty'];
        
        $zpidPath = 'response/url';
        for($i = 0; $i < $arrSize; $i++) {
            $xmlFile = simplexml_load_file($urlArr[$i]);
            if($xmlFile->xpath($zpidPath)) {
                $picArr[ $picArrNames[$i] ] = $xmlFile->xpath($zpidPath)[0] . "";
            }
        }
        
        return $picArr;
    }
    
    /******************************************/

    $zillowQuery = getURL();
    $xmlFile = getXMLFile($zillowQuery);
    if(!$xmlFile) {
        $dataArr = ['hasResults' => 'false'];
        echo json_encode($dataArr);
    }
    else {
        $dataArr = createTable($xmlFile);
        echo json_encode($dataArr);
    }
?>
