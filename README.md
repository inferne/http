# Http
A Http Client<br/>
In development

一个demo
```php
<?php
                               
dl("http.so");                 

$url = "https://www.baidu.com/";
$http = new Http($url);        

echo $http->get();             
echo "\n";                     
echo $http->code."\n";         
exit();
```
输出
```php
b39<!DOCTYPE html>
<!--STATUS OK-->

200
```
看来百度有限制
