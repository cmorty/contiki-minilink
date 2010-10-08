#!/bin/sh

cat <<EOF
var senderid = $2;
var targetaddress = "$3";
var filename = "$4";
var datafile = [
EOF
uuencode -m $1 data | sed -e 's/^/"/' -e 's/$/",/'
cat <<EOF
];
size = 
EOF
wc -c $1 | awk '{print $1}'
cat <<EOF
;
var lastpercentage = 0;
var line;


YIELD_THEN_WAIT_UNTIL(id == senderid && msg.search(/root@/) != -1);

write(mote, "sendfile " + targetaddress + " " + filename + " " + size);
log.log("0%");


for(line = 0; line <= datafile.length; line++){
	YIELD_THEN_WAIT_UNTIL(id == senderid && msg.equals("."));
	write(mote, datafile[line]);
	log.log(" " + (line * 100 / datafile.length) % 1 + "%\n" );
}
EOF
