static const char PROGMEM INDEX_HTML[] = R"rawliteral(
  <!DOCTYPE HTML><html>
<!-- Based on:
Rui Santos - Complete project details at https://RandomNerdTutorials.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files.
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. -->
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <!-- https://code.highcharts.com/highcharts.js -->
  <script src="https://code.highcharts.com/highcharts.js"></script>

  <style>
    body {
      min-width: 310px;
    	max-width: 800px;
    	height: 400px;
      margin: 0 auto;
    }
    h2 {
      font-family: Arial;
      font-size: 2.5rem;
      text-align: center;
    }
  </style>
</head>
<body>
  <h2>Data Logger</h2>
  <table>
    <tr>
      <td>Temperature</td><td id="cellTE">n/a</td>
      <td>Humidity</td><td id="cellHU">n/a</td>
    </tr>
  </table>
  <p>
  <div id="chart" class="container"></div>
  <script>
function submitForm(oFormElement)
{
  var xhr = new XMLHttpRequest();
  xhr.onload = function(){ alert(xhr.responseText); }
  xhr.open(oFormElement.method, oFormElement.getAttribute("action"));
  xhr.send(new FormData(oFormElement));
  return false;
}

var chart = new Highcharts.Chart({
  chart:{ renderTo : 'chart' },
  title: { text: 'Proofer Conditions' },
  colors: ['red', 'green', 'blue', 'purple', 'brown'],
  time: {
      timezoneOffset: 3*60
  },
  series: [{
    name: 'temperature',
    type: 'spline',
    yAxis: 0
  }, {
    name: 'humidity',
    type: 'spline',
    yAxis: 1
  }],
  xAxis: [{ type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  }],
  yAxis: [{ // left temperature
    labels: {
      format: '{value}',
      style: {
        color: Highcharts.getOptions().colors[1]
      }
    },
    title: { text: 'Temperature (C)' }
    }, { // right humidity
      labels: {
        format: '{value}',
        style: {
          color: Highcharts.getOptions().colors[2]
        }
      },
      title: { text: 'Humidity (%)' },
      opposite: true
  }],
  credits: { enabled: false }
});
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var dataObj = JSON.parse(this.responseText)
      var time = 1000* dataObj.TIME, // time is in milliseconds
          temperatureValue = dataObj.TE,
          humidityValue = dataObj.HU;
      //console.log(this.responseText);
      if(chart.series[0].data.length > 500) {
        chart.series[0].addPoint([time, temperatureValue], true, true, true);
        chart.series[1].addPoint([time, humidityValue], true, true, true);
      } else {
        chart.series[0].addPoint([time, temperatureValue], true, false, true);
        chart.series[1].addPoint([time, humidityValue], true, false, true);
      }
      if (dataObj.TE !== "undefined") {
        cellTE.innerHTML = dataObj.TE.toFixed(1);
      }
      if (dataObj.HU !== "undefined") {
        cellHU.innerHTML = dataObj.HU.toFixed(1);
      }
    }
  };
  xhttp.open("GET", "/data", true);
  xhttp.send();
}, 5000 ) ;

</script>
</body>
</html>
)rawliteral";
