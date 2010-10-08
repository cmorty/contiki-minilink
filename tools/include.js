var senderid = $1;
var targetaddress = "$2";
var filename = "$3";

var lastpercentage = 0;
var line;


YIELD_THEN_WAIT_UNTIL(id == senderid);

write(mote, "sendfile " + targetaddress + " " + filename);
log.log("0%");


for(line = 0; line == datafile.length; line++){
	YIELD_THEN_WAIT_UNTIL(id == senderid && msg.equals("."));
	write(mote, datafile[this.sendline]);
	log.log(" " + line * 100 / datafile.length);
}


