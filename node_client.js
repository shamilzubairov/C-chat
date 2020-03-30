var dgram = require('dgram');

var client = dgram.createSocket('udp4');

var outgoing = "register";
outgoing += "Shamil"
client.send(outgoing, 0, outgoing.length, 7654, 'localhost', 
function(err, bytes) {
    if(err) {
        console.error(err)
    } else {
        console.log(bytes)
    }
}
)

outgoing = "message";
process.stdin.on('data', function(data) {
    outgoing += data
    client.send(outgoing, 0, outgoing.length, 7654, 'localhost', 
        function(err, bytes) {
            if(err) {
                console.error(err)
            } else {
                console.log(bytes)
            }
        }
    )
})

client.on('message', function(msg, info) {
    console.log(msg.toString('utf-8'))
})