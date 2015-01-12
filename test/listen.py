import zmq
import struct

c = zmq.Context()
s = c.socket(zmq.SUB)
s.connect("tcp://localhost:7678")
s.setsockopt(zmq.SUBSCRIBE, "")
while True:
    print "Node ID:", struct.unpack("<I", s.recv())
    print "Tx hash:", s.recv().encode("hex")

