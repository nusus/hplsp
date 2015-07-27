/*
 * 5-10.cpp
 *
 *  Created on: 2015年7月24日
 *      Author: duoyi
 */
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if(argc<=2)
	{
		printf("usage :%s ip_address port_number ",basename(argv[0]));
		return 1;
	}

	const char* ip = argv[1];
	int port  = atoi(argv[2]);

	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address)));

}




