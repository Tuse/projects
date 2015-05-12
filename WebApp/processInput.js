var request = false;
var remHost = 'http://localhost:8888/php/index.php';//'http://hw8env.elasticbeanstalk.com';
var async = false; //false only for debugging purposes
var numrows = 8;
var homelink;
var homename;
var description;
var smallPicUrl;
var loggedIn = false;

function callApp() {
        FB.ui({
            method: 'feed',
            link: homelink,
            caption: 'Property information from Zillow.com',
            name: homename,
            picture: smallPicUrl,
            description: description,
        }, function(response){
            if (response && !response.error_code) {
              alert('Posting completed.');
            }
        });
    } 

function validateInput() {
    $('#input').valid();
}

function getInput() {
    var url = remHost.substr(0, remHost.length) + '/?';
    var spaceReplacement = '%20';
    var varSeparator = '&';
    
    var street =  document.getElementById("street").value.replace(' ', spaceReplacement);
    var city = document.getElementById("city").value.replace(' ', spaceReplacement);
    var state = document.getElementById("selectState").value;
    url += "streetInput=" + street + varSeparator;
    url += "cityInput=" + city + varSeparator;
    url += "stateInput=" + state;
    
    return url;
}

function sendData() {
        
    var url = getInput();
    
    if(window.XMLHttpRequest) {
        try {
            request = new XMLHttpRequest();
        } catch(e) {}
    }
    else if(window.ActiveXObject) {
          try {
          request = new ActiveXObject("Msxml2.XMLHTTP");
          } catch(e) {
            try {  request = new ActiveXObject("Microsoft.XMLHTTP");
            } catch(e) {   request = false; }
          }
    }
    
    if(request) {
        request.onreadystatechange = formatReturnedData;
        request.open("GET", url, true);
        request.send();
    }
}

function formatReturnedData() {
    if(request.readyState != 4 || request.status != 200) {
        return;
    }
    
    var data = JSON.parse(request.responseText);
    if(data['hasResults'] == "false") {
        hideTabs();
        showNoResults();
        return false;
    }
    else {
        hideNoRes();
    }
    
    createTable(data);
    addPictures(data);
    getCaption(data);
    showTabs();
}

function showNoResults() {
    if(document.getElementById('noResMsg')) {
        return;   
    }
    var msg = document.createElement('p');
    $(msg).attr('id', 'noResMsg');
    msg.innerHTML = "No exact match found. -- Verify that the given address is correct.";
    $('#msgDiv').append(msg);   
}

function hideNoRes() {
    var noRes = document.getElementById('noResMsg');
    if(noRes) {
        document.getElementById('msgDiv').removeChild(noRes);
    }
}

function getCaption(json) {
    var captionAddress = document.createElement('p');
    captionAddress.innerHTML = '<i>' + json['homeName'] + '<i>';
    captionAddress.setAttribute("class","caption-subtitle");
    $('.carousel-caption').append(captionAddress);
}

function createTable(json) {
    var alreadyExists = false;
    var bottom = document.getElementById("tableDiv");
    var oldtable = document.getElementById("infoTable")
    if(oldtable) {
        alreadyExists = true;
    }
    var table = document.createElement("table");
    table.setAttribute("class", "table table-striped");
    table.setAttribute("id", "infoTable");
    var tableHTML = "";
    
    //add title
    tableHTML += json["searchString"];
    
    //add the individual rows
    for(var i = 1; i <= numrows; i++) {
        var curr = "row" + i;
        tableHTML += json[curr];
    }
    
    table.innerHTML = tableHTML;
    if(alreadyExists) {
        oldtable.parentNode.removeChild(oldtable);
    } else {
        addFooter(json);
    }
    bottom.appendChild(table);
    
    addButton(json);
}

function addFooter(json) {
    document.getElementById("footer").innerHTML = json["footer"];
}

function addPictures(json) {
    $('#img1').attr("src", json['1year']);
    $('#img2').attr("src", json['5year']);
    $('#img3').attr("src", json['10year']);
}

function addButton(json) {
    var divider = document.createElement('td');
    $(divider).attr('colspan', '1');
    homelink = json['homeLink'];
    homename = json['homeName'];
    description = json['lastSoldPrice'] + " " + json['30DaysChange'];
    //divider.innerHTML = "<div style='float:right;' class='fb-share-button' data-href='" + link + "' data-layout='icon_link'></div>"
    
    var img = document.createElement('img');
    $(img).attr('src', 'imgs/fbButton.png');
    $(img).attr('width', '60px'); 
    $(img).attr('style', 'border-radius:0.3em; position:relative; float:right;'); 
    
    $(img).attr('onclick', 'checkLoginState()'); 
    
    $(divider).append(img);
    $('#infoRow').append(divider);
}

function showTabs() {
    var tabs = document.getElementById("hidden");   
    tabs.setAttribute("style", "display:block");
}

function hideTabs() {
    var tabs = document.getElementById("hidden");   
    tabs.setAttribute("style", "display:none");
}

$(document).ready(function() {
    $("#input").validate({
        rules: {
            street:"required",
            city:"required",
            selectState:"required",
        },
        messages: {
            street:"This field is required",
            city:"This field is required",
            selectState:"This field is required",
        },
        //errorContainer: "#streetError, #cityError, #stateError",
            
        errorPlacement: function(error, element) {
            error.appendTo(element.next());
        },
    
        submitHandler: function(form) {
            sendData();
        },
        //invalidHandler: function
    });
    
    $('#myCarousel').css('display', 'none');
    $('#zestimateTab').click(function () {
        $('#myCarousel').css('display', 'block');
    });
    
    $('#basicTab').click(function () {
        $('#myCarousel').css('display', 'none');
    });
    
    //CAROUSEL
    
    $('.right').click(function () {
        $('#myCarousel').carousel('next');
    });  
    $('.left').click(function () {
        $('#myCarousel').carousel('prev');
    });
});
