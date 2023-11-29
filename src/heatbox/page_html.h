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
  <h2>Heat Flower</h2>
  <table>
    <tr>
      <td>Inlet Temperature</td><td id="cellTI">n/a</td>
      <td>Outlet Temperature</td><td id="cellTO">n/a</td>
      <td>Performance Metric</td><td id="cellPM">n/a</td>
    </tr>
  </table>
  <p>
  <div id="chart" class="container"></div>
  
  <input type="range" min="0" max="100" value="50" class="slider" id="fanslider">
  Fan Speed <output id="fanspeed">n/a</output>

  <script>

document.getElementById("fanslider").oninput = function() {
  var value = document.getElementById("fanslider").value;
  document.getElementById("fanspeed").innerHTML = value; 

  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/setfan')
  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded')
  xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest')
  xhr.send("fanspeed=" + value);
  return false;
}
;

var chart = new Highcharts.Chart({
  chart:{ renderTo : 'chart' },
  title: { text: 'Heater Conditions' },
  colors: ['red', 'green', 'blue', 'purple', 'brown'],
  time: {
      timezoneOffset: 4*60
  },
  series: [{
    name: 'inTemperature',
    type: 'spline',
    yAxis: 0
  }, {
    name: 'outTemperature',
    type: 'spline',
    yAxis: 0
  }, {
    name: 'perfMetric',
    type: 'spline',
    yAxis: 1
  }],
  xAxis: [{ type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  }],
  yAxis: [{
    labels: {
      format: '{value}',
      style: {
        color: Highcharts.getOptions().colors[1]
      }
    },
    title: { text: 'Temperature (C)' }
  }, { // right perfomance
      labels: {
        format: '{value}',
        style: {
          color: Highcharts.getOptions().colors[3]
        }
      },
      title: { text: 'Performance (speed-dT)' },
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
          inTemperatureValue = dataObj.TEin,
          outTemperatureValue = dataObj.TEout;
          fanSpeed = dataObj.Fan;
          perfMetric = dataObj.PM;
      //console.log(this.responseText);
      if(chart.series[0].data.length > 500) {
        chart.series[0].addPoint([time, inTemperatureValue], true, true, true);
        chart.series[1].addPoint([time, outTemperatureValue], true, true, true);
        chart.series[2].addPoint([time, perfMetric], true, true, true);
      } else {
        chart.series[0].addPoint([time, inTemperatureValue], true, false, true);
        chart.series[1].addPoint([time, outTemperatureValue], true, false, true);
        chart.series[2].addPoint([time, perfMetric], true, false, true);
      }
      if (dataObj.TEin !== "undefined") {
        cellTI.innerHTML = dataObj.TEin.toFixed(1);
      }
      if (dataObj.TEout !== "undefined") {
        cellTO.innerHTML = dataObj.TEout.toFixed(1);
      }
      if (dataObj.PM !== "undefined") {
        cellPM.innerHTML = dataObj.PM.toFixed(1);
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
