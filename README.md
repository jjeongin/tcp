# tcp
TCP implementation from scratch in C

# pseudocode for Task 2
```
WINDOW_SIZE = CWND (initially 1.0 (float))

while:
if Slow-start (CWND < ssthresh):
CWND += 1 every time an ACK is received
if Packet Loss:
	ssthresh = max(CWND/2, 2) 
	CWND = 1.0
else if CWND == ssthresh (initially 64):
	go to Congestion Avoidance

if Congestion Avoidance (CWND >= ssthresh):
CWND = floor(CWND + 1/CWND) every time an ACK is received 
if 3 Dup ACKs:
	go to Fast Retransmit

if Fast Retransmit:
	ssthresh = max(CWND/2, 2) 
	CWND = 1
	go to Slow Start
```
