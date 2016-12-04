var SerialPort = require("serialport");
var util = require("util");
var async = require("async");
var events = require("events");

function SolenoidInstrument() {
  events.EventEmitter.call(this);
  this.messageQueue = async.queue(this.sendMessage.bind(this), 1);
  this.messageQueue.resume();
  this.portList = [];
}

util.inherits(SolenoidInstrument, events.EventEmitter);

SolenoidInstrument.prototype.playNote = function(note, channel) {
  var b = new Buffer(2);
  b.writeUInt8(parseInt(note, 10), 0);
  b.writeUInt8(parseInt(channel, 10), 1);
  console.log("note, channel", note, channel);
  this.pushMessage( b );
};

SolenoidInstrument.prototype.listPorts = function() {
  SerialPort.list(function (err, ports) {
    if (err) { console.log(err); }
    ports = ports.map(function(port){ return port.comName; });
    // put the most likely candidate first
    ports = ports.sort(function(a, b) {
      return b.indexOf("usbmodem") - a.indexOf("usbmodem");
    });
    this.portList = ports;
    this.emit("listedPorts", ports);
  }.bind(this));
};

SolenoidInstrument.prototype.connect = function(port) {
  this.serial = new SerialPort(port, {baudrate: 115200, autoOpen: false});
  this.serial.on("data", function(a) {console.log(a.toString()); });
  this.serial.open(function(err) {
    if (err) { console.log(err); }
    this.emit("connected");
  }.bind(this));
};

SolenoidInstrument.prototype.disconnect = function() {
  if (this.serial) {
    this.serial.close();
    this.emit("disconnected");
  }
};

SolenoidInstrument.prototype.pushMessage = function(message) {
  this.messageQueue.push({message: message});
};

SolenoidInstrument.prototype.sendMessage = function(messageObject, callback) {
  var me = this;
  console.log("=====>", messageObject);
  this.serial.write(messageObject.message, function(err, results) {
    if (err) { console.log(err); }
    me.serial.drain(function() {
      me.emit("sentMessage", messageObject);
      callback();
    });
  });
};

module.exports = SolenoidInstrument;
