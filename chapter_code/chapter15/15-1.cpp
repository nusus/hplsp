/*
 * 15-1.cpp
 *
 *  Created on: 2015年8月2日
 *      Author: root
 */

#include <assert.h>
#include <bits/socket_type.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

class CProcess
{
public:
	CProcess ( );

	pid_t m_nPid;
	int m_nPipefd [ 2 ];

private:
};

inline CProcess::CProcess ( )
		: m_nPid ( -1 )
{
}

template < typename T >
class CProcessPool
{
private:
	CProcessPool ( int listenfd , int process_num = 8 );
public:
	static CProcessPool < T >* create ( int listenfd ,
			int process_number = 8 );
	~CProcessPool ( );

	void run ( );
private:
	void setup_sig_pipe ( );
	void run_parent ( );
	void run_child ( );

private:
	static const int MAX_PROCESS_NUMBER = 16;
	static const int USER_PER_PROCESS = 65536;
	static const int MAX_EVENT_NUMBER = 10000;
	int m_nProcessNumber;
	int m_nIndex;
	int m_nEpollfd;
	int m_nListenfd;
	bool m_bStop;
	CProcess m_pSubProcess;
	static CProcessPool < T >* m_sInstance;

};

template < typename T >
CProcessPool < T >* CProcessPool < T >::m_sInstance =
		nullptr;

static int sig_pipefd [ 2 ];

static int setnoblocking ( int fd )
{
	int old_option = fcntl ( fd , F_GETFL );
	int new_option = old_option | O_NONBLOCK;
	fcntl ( fd , F_SETFL , new_option );
	return old_option;
}

static void addfd ( int epollfd , int fd )
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl ( epollfd , EPOLL_CTL_ADD , fd , &event );
	setnoblocking ( fd );
}

static void removefd ( int epollfd , int fd )
{
	epoll_ctl ( epollfd , EPOLL_CTL_DEL , fd , 0 );
	close ( fd );

}

static void sig_handler ( int sig )
{
	int save_errno = errno;
	int msg = sig;
	send ( sig_pipefd [ 1 ] , ( char* ) &msg , 1 , 0 );
	errno = save_errno;

}

static void addsig ( int sig , void (handler) ( int ) ,
		bool restart = true )
{
	struct sigaction sa;
	memset ( &sa , '\0' , sizeof ( sa ) );
	sa.sa_handler = handler;
	if ( restart )
	{
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset ( &sa.sa_mask );
	assert ( sigaction ( sig , &sa , NULL ) != -1 );
}

template < typename T >
inline CProcessPool < T >* CProcessPool < T >::create (
		int listenfd , int process_number )
{
	if ( !m_sInstance )
	{
		m_sInstance = new CProcessPool < T > ( listenfd ,
				process_number );
	}
	return m_sInstance;
}

template < typename T >
inline CProcessPool < T >::~CProcessPool ( )
{
	delete [ ] ~m_pSubProcess;
}

template < typename T >
inline void CProcessPool < T >::run ( )
{
	if ( m_nIndex != -1 )
	{
		run_child ( );
		return;
	}
	run_parent ( );
}

template < typename T >
inline void CProcessPool < T >::setup_sig_pipe ( )
{
	m_nEpollfd = epoll_create ( 5 );
	assert ( m_nEpollfd != -1 );

	int ret = socketpair ( PF_UNIX , SOCK_STREAM , 0 ,
			sig_pipefd );
	assert ( ret != -1 );

	setnoblocking ( sig_pipefd [ 1 ] );
	addfd ( m_nEpollfd , sig_pipefd [ 0 ] );
	addsig ( SIGCHLD , sig_handler );
	addsig ( SIGTERM , sig_handler );
	addsig ( SIGINT , sig_handler );
	addsig ( SIGPIPE , SIG_IGN );
}

template < typename T >
inline void CProcessPool < T >::run_parent ( )
{
	setup_sig_pipe ( );
	int pipefd = m_pSubProcess [ m_nIndex ].m_nPipefd [ 1 ];
	addfd ( m_nEpollfd , pipefd );

	epoll_event events [ MAX_EVENT_NUMBER ];
	T* users = new T [ USER_PER_PROCESS ];
	assert ( users );
	int number = 0;
	int ret = -1;
	while ( !m_bStop )
	{
		number = epoll_wait ( m_nEpollfd , events ,
				MAX_EVENT_NUMBER , -1 );
		if ( ( number < 0 ) && ( errno != EINTR ) )
		{
			printf ( "epoll failure\n" );
			break;
		}

		for ( int i = 0 ; i < number ; ++i )
		{
			int sockfd = events [ i ].data.fd;
			if ( ( sockfd == pipefd )
					&& ( events [ i ].events & EPOLLIN ) )
			{
				int client = 0;
				ret = recv ( sockfd , ( char* ) &client ,
						sizeof ( client ) , 0 );
				if ( ( ( ret < 0 ) && ( errno != EAGAIN ) )
						|| ret == 0 )
				{
					continue;
				} else
				{
					struct sockaddr_in client_address;
					socklen_t client_addrlength =
							sizeof ( client_address );
					int connfd =
							accept ( m_nListenfd ,
									( struct sockaddr* ) &client_address ,
									&client_addrlength );
					if ( connfd < 0 )
					{
						printf ( "errno is %d\n" , errno );
						continue;
					}
					addfd ( m_nEpollfd , connfd );
					users [ connfd ].init ( m_nEpollfd ,
							connfd , client_address );
				}
			} else if ( ( sockfd == sig_pipefd [ 0 ] )
					&& ( events [ i ].events & EPOLLIN ) )
			{
				int sig;
				char signals [ 1024 ];
				ret = recv ( sig_pipefd [ 0 ] , signals ,
						sizeof ( signals ) , 0 );
				if ( ret <= 0 )
				{
					continue;
				} else
				{
					for ( int i = 0 ; i < ret ; ++i )
					{
						switch ( signals [ i ] )
						{
							case SIGCHLD :
							{
								pid_t pid;
								int stat;
								while ( ( pid = waitpid (
										-1 , &stat ,
										WNOHANG ) ) > 0 )
								{
									continue;
								}
								break;
							}
						}
					}
				}
			}
		}
	}
}

template < typename T >
inline CProcessPool < T >::CProcessPool ( int listenfd ,
		int process_num )
		:
				m_nListenfd ( listenfd ),
				m_nProcessNumber ( process_num ),
				m_nIndex ( -1 ),
				m_bStop ( false )
{
	assert (
			process_num > 0
					&& process_num < MAX_PROCESS_NUMBER );

	m_pSubProcess = new CProcess [ process_num ];
	assert ( m_pSubProcess );

	for ( int i = 0 ; i < process_num ; ++i )
	{
		int ret =
				socketpair ( PF_UNIX , SOCK_STREAM , 0 ,
						( int* ) ( m_pSubProcess [ i ].m_nPipefd ) );
		assert ( ret == 0 );
		m_pSubProcess [ i ].m_nPid = fork ( );
		assert ( m_pSubProcess [ i ].m_nPid >= 0 );
		if ( m_pSubProcess [ i ].m_nPid > 0 )
		{
			close ( m_pSubProcess [ i ].m_nPipefd [ 1 ] );
			continue;
		} else
		{
			close ( m_pSubProcess [ i ].m_nPipefd [ 0 ] );
			m_nIndex = i;
			break;
		}

	}
}

template < typename T >
inline void CProcessPool < T >::run_child ( )
{
}

