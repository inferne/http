<?php
$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('http')) {
	dl('http.' . PHP_SHLIB_SUFFIX);
}
$module = 'http';
$functions = get_extension_funcs($module);
echo "Functions available in the test extension:$br\n";
foreach($functions as $func) {
    echo $func."$br\n";
}
echo "$br\n";
$function = 'confirm_' . $module . '_compiled';
if (extension_loaded($module)) {
	$str = $function($module);
} else {
	$str = "Module $module is not compiled into PHP";
}
echo "$str\n";
<?php 

/**
 * 
 * @author liyunfei
 * @version 1.0
 * @desc this class support http long connect
 */

class Http
{
    public  $port       = 80;
    public  $timeout    = 1;
    private $length     = 8196;
    public  $url;
    private $user_agent = "your agent";
    public  $http_code;
    public  $header;//header infomation

    public function __construct(){}

    public function get($params = array()){
        $query = (isset($this->url['query']) ? $this->url['query']."&" : "?") . http_build_query($params);
        $context  = "GET ".$this->url['path'].$query." HTTP/1.1\r\n";
        $context .= $this->http_build_header();
        $context .= "\r\n";

        $result = $this->request($context);
        return $result;
    }

    public function post($params = array()){
        $data = http_build_query($params);
        $query = $this->url['query'];
        $context  = "POST ".$this->url['path'].$query." HTTP/1.1\r\n";
        $context .= $this->http_build_header();
        $context .= "Content-Length: ".strlen($data)."\r\n\r\n";

        if(strlen($data) > 0){
            $context .= $data."\r\n\r\n";
        }
        //echo $context;
        $result = $this->request($context);
        return $result;
    }

    private function http_build_header(){
        $context  = "";
        if($this->header){
            $context = implode("\r\n", $this->header)."\r\n";
        }
        if(!isset($this->header["Accept"])) {
            $context .= "Accept: */*\r\n";
        }
        $context .= "Host: ".$this->url['host']."\r\n";
        if(!isset($this->header["User-Agent"])) {
            $context .= "User-Agent: ".$this->user_agent."\r\n";
        }
        if(!isset($this->header["Content-Type"])) {
            $context .= "Content-Type: application/x-www-form-urlencoded\r\n";
        }
        if(!isset($this->header["Connection"])) {
            $context .= "Connection: Keep-Alive\r\n";
        }
        return $context;
    }

    /**
     * send http request
     * @param unknown $context
     */
    private function request($context){
        $fp = stream_socket_client("tcp://".$this->url['host'].":".$this->port, $errno, $errstr, $this->timeout, STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
        //send context to server
        fwrite($fp, $context);
        //read server response
        $response = fread($fp, $this->length);
        //Transfer-Encoding: chunked
        if(strpos($response, "Transfer-Encoding: chunked") > 0){
            $chunked = true;
        }else{
            $chunked = false;
        }
        while (!feof($fp)){
            if($chunked){
                if(substr($response, -5) == "0\r\n\r\n"){
                    break;
                }
            }else{//Content-Length
                if (strlen($response) % $this->length){
                    break;
                }
            }
            $response .= fread($fp, $this->length);
        }
        //when service close Connection
        if(strpos("Connection: close", $response)){
            fclose($fp);
        }
        
        return $this->parse($response);
    }
    
    private function parse($response){
        $response = explode("\r\n\r\n", $response);
    
        $header = $response[0];
        $this->http_code = substr($header, 9, 3);
    
        $package = explode("\r\n", $response[1]);
        $data = "";
        for($i = 0; $i < count($package); $i+=2){
            if(hexdec($package[$i]) > 0){
                $data .= $package[$i+1];
            }
        }
    
        return $data;
    }
    
}
