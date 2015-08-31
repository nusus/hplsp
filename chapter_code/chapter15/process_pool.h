/*
 * process_pool.h
 *
 *  Created on: 2015年8月28日
 *      Author: duoyi
 */

#ifndef PROCESS_POOL_H_
#define PROCESS_POOL_H_

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <signal.h>
#include <assert.h>
#include <sys/wait.h>
struct Process {
	pid_t pid_;
	int pipefd_[2];
	Process() :
			pid_(-1) {
	}
};

template<class T>
class ProcessPool {
public:
private:
	ProcessPool(int listenfd, int process_number = 8);
public:
	static ProcessPool* create(int listenfd, int process_number = 8) {
		if (!g_instance) {
			g_instance = new ProcessPool<T>(listenfd, process_number);

		}
		return g_instance;
	}
	~ProcessPool() {
		delete[] sub_process_;
	}

	void Run();
private:
	void SetupPipe();
	void RunParent();
	void RunChild();

private:
	static const int g_kMaxProcessNumber = 16;
	static const int g_kUserPerProcess = 65536;
	static const int g_kMaxEventNumber = 1000;
	int process_number_;
	int index_;
	int epollfd_;
	int listenfd_;
	int stop_;
	Process* sub_process_;
	static ProcessPool<T>* g_instance;
};

template<typename T>
ProcessPool<T>* ProcessPool<T>::g_instance = NULL;

static int g_sig_pipefd[2];

static int SetNoblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

static void Removefd(int epoll_fd, int fd) {
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

static void Addfd(int epollfd, int fd) {
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	SetNoblocking(fd);
}

static void SigHandler(int sig) {
	int save_errno = errno; // 保存原来的errno，再函数的最后恢复，保证函数的可重入性。
	int msg = sig;
	send(g_sig_pipefd[1], (char*) &msg, 1, 0);
	errno = save_errno;
}

static void AddSignal(int sig, void (handler)(int), bool restart = true) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.__sigaction_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

template<class T>
inline ProcessPool<T>::ProcessPool(int listenfd, int process_number) :
		listenfd_(listenfd), process_number_(process_number), index_(-1), stop_(
				false) {
	assert(process_number > 0 && process_number <= g_kMaxProcessNumber);
	sub_process_ = new Process[process_number_];
	assert(sub_process_);
	for (int i = 0; i < process_number_; ++i) {
		int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sub_process_[i].pipefd_);
		assert(ret == 0);
		sub_process_[i].pid_ = fork();
		assert(sub_process_[i].pid_ >= 0);
		if (sub_process_[i].pid_ > 0) {
			close(sub_process_[i].pipefd_[1]);
			continue;
		} else {
			close(sub_process_[i].pipefd_[0]);
			index_ = i;
			break;
		}
	}
}

template<class T>
inline void ProcessPool<T>::Run() {
	if (index_ != -1) {
		RunChild();
		return;
	}
	RunParent();
}

template<class T>
inline void ProcessPool<T>::SetupPipe() {
	epollfd_ = epoll_create(5);
	assert(epollfd_ != -1);
	int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, g_sig_pipefd);
	assert(ret != -1);
	SetNoblocking(g_sig_pipefd[1]);
	Addfd(epollfd_, g_sig_pipefd[0]);
	AddSignal(SIGCHLD, sig_handler);
	AddSignal(SIGINT, sig_handler);
	AddSignal(SIGTERM, sig_handler);
	AddSignal(SIGPIPE, SIG_IGN);
}

template<class T>
inline void ProcessPool<T>::RunParent() {
	SetupPipe();

	Addfd(epollfd_, listenfd_);

	epoll_event events[g_kMaxEventNumber];
	int sub_process_counter = 0;
	int new_conn = 1;
	int number = 0;
	int ret = -1;

	while (!stop_) {
		number = epoll_wait(epollfd_, events, g_kMaxEventNumber, -1);
		if (number < 0 && errno != EINTR) {
			printf("epoll failure\n");
			break;
		}
		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd_) {
				int i = sub_process_counter;
				do {
					if (sub_process_[i].pid_ != -1) {
						break;
					}
					i = (i + 1) % process_number_;
				} while (i != sub_process_counter);
				if (sub_process_[i].pid_ == -1) {
					stop_ = true;
					break;
				}
				sub_process_counter = (i + 1) % process_number_;
				send(sub_process_[i].pipefd_[0], (char*) &new_conn,
						sizeof(new_conn), 0);
				printf("send request to child %d\n", i);

			} else if (sockfd == g_sig_pipefd[0]
					&& (events[i].events & EPOLLIN)) {
				int sig;
				char signals[1024];
				ret = recv(g_sig_pipefd[0], signals, sizeof(signals), 0);
				if (ret <= 0)
					continue;
				else {
					for (int i = 0; i < ret; ++i) {
						switch (signals[i]) {
						case SIGCHLD: {
							pid_t pid;
							int stat;
							while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
								for (int i = 0; i < process_number_; ++i) {
									if (sub_process_[i].pid_ == pid) {
										printf("child %d join\n", i);
										close(sub_process_[i].pipefd_[0]);
										sub_process_[i].pid_ = -1;
									}
								}
							}
							stop_ = true;
							for (int i = 0; i < process_number_; ++i) {
								if (sub_process_[i] != -1) {
									stop_ = false;
								}
							}
							break;
						}
						case SIGTERM: {
							break;
						}
						case SIGINT: {
							printf("kill all the children now\n");
							for (int i = 0; i < process_number_; ++i) {
								int pid = sub_process_[i].pid_;
								if (pid != -1) {
									kill(pid, SIGTERM);
								}
							}
							break;
						}
						default:
							break;
						}
					}
				}
			} else {
				continue;
			}
		}

	}
	close(epollfd_);
}

template<class T>
inline void ProcessPool<T>::RunChild() {
	SetupPipe();

	int pipefd = sub_process_[index_].pipefd_[1];
	Addfd(epollfd_, pipefd);
	epoll_event events[g_kMaxEventNumber];
	T* users = new T[g_kUserPerProcess];
	assert(users);
	int number = 0;
	int ret = -1;
	while (!stop_) {
		number = epoll_wait(epollfd_, events, g_kMaxEventNumber, -1);
		if ((number > 0) && (errno != EINTR)) {
			printf("epoll failure\n");
			break;
		}

		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if (sockfd == pipefd && (events[i].events & EPOLLIN)) {
				int client = 0;
				ret = recv(sockfd, (char*) &client, sizeof(client), 0);
				if (((ret < 0) && (errno != EAGAIN)) || ret == 0) {
					continue;
				} else {
					struct sockaddr_in client_address;
					socklen_t client_addresslen = sizeof(client_address);
					int connfd = accept(listenfd_,
							(struct sockaddr*) &client_address,
							&client_addresslen);
					if (connfd < 0) {
						printf("errno is : %d\n", errno);
						continue;
					}
					addfd(epollfd_, connfd);
					users[connfd].Init(epollfd_, connfd, client_address);
				}
			} else if ((sockfd == g_sig_pipefd[0])
					&& (events[i].events & EPOLLIN)) {
				int sig;
				char signals[1024];
				ret = recv(g_sig_pipefd[0], signals, sizeof(signals), 0);
				if (ret <= 0) {
					continue;
				} else {
					for (int i = 0; i < ret; ++i) {
						switch (signals[i]) {
						case SIGCHLD: {
							pid_t pid;
							int stat;
							while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
								continue;
							}
							break;
						}
						case SIGTERM: {
							break;
						}
						case SIGINT: {
							stop_ = true;
							break;
						}
						default: {
							break;
						}
						}
					}
				}
			} else if (events[i].events & EPOLLIN) {
				users[sockfd].process();
			} else {
				continue;
			}
		}
	}

	delete[] users;
	users = NULL;
	close(pipefd);
	close(epollfd_);
}

#endif /* PROCESS_processPOOL_H_ */
