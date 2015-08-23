/*
 * http_conn.cpp
 *
 *  Created on: 2015年8月23日
 *      Author: root
 */
#include "http_conn.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form =
		"Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form =
		"You do not have permission to get file from this server,\n";
const char* error_404_title = "Not Found";
const char* error_404_form =
		"The requested file was not found on this server.\n";
const char* error_500_title = "Internal error";
const char* error_500_form =
		"There was an unusual problem serving the requested file.\n";
const char* doc_root = "/var/www/html";

int setnoblocking(int sockfd) {
	int old_option = fcntl(sockfd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epollfd, int fd, bool one_shot) {
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (one_shot) {
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnoblocking(fd);

}

void removefd(int epollfd, int fd) {
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void modfd(int epollfd, int fd, int ev) {
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

http_conn::http_conn() {
}

http_conn::~http_conn() {
}

void http_conn::init(int sockfd, const sockaddr_in& addr) {
	this->m_sockfd = sockfd;
	m_address = addr;

	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	addfd(m_epollfd, m_sockfd, true);
	m_user_count++;

	init();
}

void http_conn::close_conn(bool real_close) {
	if (real_close && m_epollfd != -1) {
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--;
	}
}

void http_conn::process() {

}

bool http_conn::read() {
}

bool http_conn::write() {
}

void http_conn::init() {
}

HTTP_CODE http_conn::process_read() {
}

bool http_conn::process_write(HTTP_CODE ret) {
}

HTTP_CODE http_conn::parse_request_line(char* text) {
}

HTTP_CODE http_conn::parse_headers(char* text) {
}

HTTP_CODE http_conn::parse_content(char* text) {
}

HTTP_CODE http_conn::de_request() {
}

LINE_STATUS http_conn::parse_line() {
}

void http_conn::unmap() {
}

bool http_conn::add_response(const char* format) {
}

bool http_conn::add_content(const char* content) {
}

bool http_conn::add_status_line(int status, const char* title) {
}

bool http_conn::add_headers(int content_length) {
}

bool http_conn::add_linger() {
}

bool http_conn::add_blank_line() {
}
