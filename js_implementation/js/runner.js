
const account_data = {
    login_data: {
        login: "",
        password: "",
        action: ""
    }
    };

const LOGIN_URL = "https://intrade.bar/login";

function PostRequest(params){

    let query = "";

    Object.keys(params).forEach(function(key, index) {
        if (index === 0) {
            query += key + "=" + params[key];
        }
        else {
            query += "&" + key + "=" + params[key];
        }
    });

    console.log("QUERY WILL BE SEND: ", "?" + query);

    let request = new XMLHttpRequest;

    request.open("POST", LOGIN_URL, true);
    request.withCredentials = true;

    request.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    request.setRequestHeader('Accept', 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8');

    request.send(query);

    //https://www.moxio.com/blog/12/how-to-make-a-cross-domain-request-in-javascript-using-cors
    request.onreadystatechange  = function() {
        //server turn off CORS support, than there no 200 status code
        //it must be CORS bypass or exist another solution
        if(request.readyState === 4 && request.status === 200) {
            console.log("RESPONSE TEXT: ", request.responseText);
        }

        console.log("RESPONSE STATE: ", request.readyState,
                    "RESPONSE CODE: ", request.status,
                    "RESPONSE TEXT: ", request.responseText);
    };

}

PostRequest(account_data.login_data);