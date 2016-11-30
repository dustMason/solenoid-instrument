var midi = require('midi');
var SolenoidInstrument = require("./solenoid-instrument.js");

var baseA4 = 440.0;

var input = new midi.input();
var instrument = new SolenoidInstrument();

instrument.on('listedPorts', function(ports) {
  console.log("Connect to", ports[0]);
  instrument.connect(ports[0]);
});
instrument.listPorts();

input.on('message', function(deltaTime, message) {
  var isNote = message[0].toString(2).substr(0,4) === "1001";
  if (isNote && message[2] > 0) {
    var channel = parseInt(message[0].toString(2).substr(4), 2);
    instrument.playNote(message[1], channel);
  }
});

input.openVirtualPort("Solenoid Instrument");
