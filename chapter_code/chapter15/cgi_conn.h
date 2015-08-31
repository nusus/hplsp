/*
 * cgi_conn.h
 *
 *  Created on: 2015年8月28日
 *      Author: duoyi
 */

#ifndef CGI_CONN_H_
#define CGI_CONN_H_

class CGIConn {
public:
	CGIConn() {
	}
	~CGIConn() {
	}

	void Init(int epollfd, int sockfd, struct sockaddr_in& client_addr) {
		epollfd_ = epollfd;
		sockfd_ = sockfd;

	}

private:
	static const int g_kBufferSize = 1024;
	static int epollfd_;
	int sockfd_;
	sockaddr_in address_;
	char buf_[g_kBufferSize];
	int read_index_;
};

int CGIConn::epollfd_ = -1;

#endif /* CGI_CONN_H_ */
