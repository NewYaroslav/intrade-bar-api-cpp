function send_data() 
{
	socket.send('[{"t":2,"e":23,"d":[{"amount":'+amount+',"dir":"'+trend+'","pair":"'+pair+'","pos":0,"source":"platform","group":"'+group+'","duration":'+time+"}]}]");
}

function getUuid(){
    return(Date.now().toString(36)+Math.random().toString(36).substr(2,12)).toUpperCase()
}

function get_cookie(cookie_name)
{
  var results = document.cookie.match ( '(^|;) ?' + cookie_name + '=([^;]*)(;|$)' );
 
  if ( results )
    return ( unescape ( results[2] ) );
  else
    return null;
}

function sec() //выполняется каждую секунду
{ 
	//console.log("sec: "+tick);
	//console.log("user_id: " + user_id);
	//console.log("user_hash: " + user_hash);
	tick++;
	
	if(is_init == false) {
		(socket=new WebSocket("wss://mr-axiano.com/fxcm2/")).onopen=function(){
			console.log("Соединение установлено.");
			is_init = true;
		},
		socket.onclose=function(e){
			if(e.wasClean) {
				log("Соединение закрыто чисто");
			} else {
				console.log("Обрыв соединения");
				socket.close();
			}
			console.log("Код: "+e.code+" причина: "+e.reason);
			// перезагрузим страницу
			//location.reload();
			is_init = false;
		},
		socket.onmessage=function(e){
			console.log("Получены данные "+e.data);
			//message_in = e.data;
		},
		socket.onerror=function(e){
			console.log("Ошибка "+e.message);
			if(is_init) socket.close();
			// перезагрузим страницу
			//location.reload();
			is_init = false;
		}
	}
	
	if(0)
	if(tick > 10) {
		tick = 0;
		console.log("Получаем баланс");
		var param = 'user_id='+user_id+'&user_hash='+user_hash;
		console.log("json_upload "+param);
		var r = new XMLHttpRequest;
		r.open("POST","https://intrade.bar/balance.php",true);
		r.withCredentials = true;
		r.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		r.setRequestHeader('Accept', '*/*');
		r.setRequestHeader('Accept-Language', 'ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.');
		r.setRequestHeader('Accept-Encoding', 'gzip, deflate, br');
		r.setRequestHeader('Referer', 'https://intrade.bar/');
		//r.setRequestHeader('X-Request-Type', 'Api-Request');
		//r.setRequestHeader('X-Request-Project', 'bo');
		r.setRequestHeader('Connection', 'keep-alive');
		r.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
		r.send(param);
		r.onreadystatechange=function() {
			if(4==r.readyState&&200==r.status) {
				//socket_control.send(r.responseText);
				console.log("ответ: " + r.responseText);
			}
		};
	}
	
}			 	

setInterval(sec, 1000);// запускать функцию каждую секунду

var tick = 0;
var is_init = false;
var is_init_api = false;
var is_init_api_control = false;
var is_upload_hist = false;
var socket;
var socket_api;
var socket_control;
var amount = 30;
var trend="down";
var pair="EURUSD";
var group="demo";
var time=60;

var user_id = get_cookie("user_id");
var user_hash = get_cookie("user_hash");