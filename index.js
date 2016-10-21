var midi = require('midi');
var SolenoidInstrument = require("./solenoid-instrument.js");

var baseA4 = 440.0;

var input = new midi.input();
var instrument = new SolenoidInstrument();

instrument.on('listedPorts', function(ports) { instrument.connect(ports[0]); });
instrument.listPorts();

input.on('message', function(deltaTime, message) {
  console.log(message);
  if (message[2] === 95) { // note on, channel 1
    // message[1] is the note number
    instrument.playNote(baseA4 * Math.pow(2, (message[1] - 57) / 12));
  }
});

input.openVirtualPort("Solenoid Instrument");
