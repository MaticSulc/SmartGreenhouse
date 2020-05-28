var switchsend = new XMLHttpRequest();
var jsonsend = new XMLHttpRequest();
var tempPlaceholder = document.getElementById("temp");
var humidityPlaceholder = document.getElementById("humidity");
//var moisturePlaceholder = document.getElementById("moisture");
var timePlaceholder = document.getElementById("time");
var wateringtimePlaceholder = document.getElementById("wateringtime");
var waterneedPlaceholder = document.getElementById("waterneed");
var fanSwitch = document.getElementById("1");
var pumpSwitch = document.getElementById("2");
var lightsSwitch = document.getElementById("3");
var manualSwitch = document.getElementById("4");
var currentdate;
var currenttime = "";
var temp;
var humidity;


function enableDisableButtons(override) {
    if (override) {
        fanSwitch.disabled = false;
        pumpSwitch.disabled = false;
        lightsSwitch.disabled = false;
    } else {
        fanSwitch.disabled = true;
        pumpSwitch.disabled = true;
        lightsSwitch.disabled = true;
    }

}


jsonsend.onload = function() {
    if (this.status == 200) {
        try {
            const response = JSON.parse(this.responseText);
            processData(response);
        } catch (e) {
            console.warn("Parsing error");
        }
    } else {
        console.warn("API Down!");
    }



}


function toggleCheckbox(element) {
    if (element.checked) {
        switchsend.open("GET", "/update?relay=" + element.id + "&state=1", true);
    } else {
        switchsend.open("GET", "/update?relay=" + element.id + "&state=0", true);
    }
    switchsend.send();
}

function processData(response) {
    tempPlaceholder.innerHTML = response.temperature + " &#176;C";
    temp = response.temperature;
    humidityPlaceholder.innerHTML = response.humidity + " %";
    humidity = response.humidity;
    //moisturePlaceholder.innerHTML = response.moisture;
    timePlaceholder.innerHTML = response.time;
    wateringtimePlaceholder.innerHTML = response.wateringtime;
    //3300+ na suhem, 3100 suha zemlja, 2200 dovolj zalito, 1300 prevec zalito, 1200-  vodi
    switch (response.moisture) {
        case (response.moisture >= 2500 && response.moisture):
            waterneedPlaceholder.innerHTML = "Rastlina je suha!";
            break;
        case (response.moisture < 2500 && response.moisture >= 1600 && response.moisture):
            waterneedPlaceholder.innerHTML = "Rastlina je dovolj zalita!";
            break;
        case (response.moisture < 1600 && response.moisture):
            waterneedPlaceholder.innerHTML = "Rastlina je preveč zalita!";
            break;
        default:
            waterneedPlaceholder.innerHTML = "ERROR";
            break;


    }
    lightsSwitch.checked = response.lightState;
    fanSwitch.checked = response.fanState;
    pumpSwitch.checked = response.pumpState;
    manualSwitch.checked = response.manualoverride;
    enableDisableButtons(response.manualoverride);

    currentdate = new Date();
    currenttime = currentdate.getFullYear() + "-" + ("0" + (currentdate.getMonth() + 1)).slice(-2) + "-" + currentdate.getDate() + " " + currentdate.getHours() + ":" + currentdate.getMinutes() + ":" + currentdate.getSeconds();
	Plotly.extendTraces('chart', {
        y: [
            [temp],
            [humidity]
        ],
        x: [
            [currenttime],
            [currenttime]
        ]
    }, [0, 1]);



}


document.addEventListener("DOMContentLoaded", function() {
    jsonsend.open("get", "/api/greenhouse", true);
    jsonsend.send();
    currentdate = new Date();
    currenttime = currentdate.getFullYear() + "-" + currentdate.getMonth() + "-" + currentdate.getDay() + " " + currentdate.getHours() + ":" + currentdate.getMinutes() + ":" + currentdate.getSeconds();
    Plotly.plot('chart', [{
            name: "Temperatura",
            y: [temp],
            x: [currenttime],
            type: 'scatter',
            showlegend: true
        },
        {
            name: "Vlažnost",
            y: [humidity],
            x: [currenttime],
            type: 'scatter',
            showlegend: true
        }
    ], {}, {
        displayModeBar: false
    })
});

setInterval(function() {
    jsonsend.open("get", "/api/greenhouse", true);
    jsonsend.send();
}, 5000);