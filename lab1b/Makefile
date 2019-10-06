#NAME: Yining Wang
#EMAIL: wangyining@g.ucla.edu
#ID: 504983099

default:
	gcc -Wall -Wextra  -g -o lab1b-client lab1b-client.c -lz
	gcc -Wall -Wextra  -g -o lab1b-server lab1b-server.c -lz

client:
	gcc -Wall -Wextra  -g -o lab1b-client lab1b-client.c -lz
	
server:
	gcc -Wall -Wextra  -g -o lab1b-server lab1b-server.c -lz


clean:
	rm -f lab1b-client lab1b-server *.tar.gz

dist:
	tar -czvf lab1b-504983099.tar.gz lab1b-client.c lab1b-server.c Makefile README
	
		
