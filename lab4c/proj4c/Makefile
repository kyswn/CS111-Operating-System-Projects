#NAME: Yining Wang
#EMAIL: wangyining@g.ucla.edu
#ID: 504983099

default:
	gcc -g -Wall -Wextra -lmraa -lm lab4c_tcp.c  -o lab4c_tcp
	gcc -g -Wall -Wextra -lmraa -lm -lssl -lcrypto lab4c_tls.c  -o lab4c_tls
dist:
	tar -czf lab4c-504983099.tar.gz lab4c_tcp.c lab4c_tls.c README Makefile 
clean:
	rm -f lab4c_tcp lab4c_tls *.gz
check:
