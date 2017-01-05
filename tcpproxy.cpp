#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>
#include "./lev-master/include/lev.h"
#include <boost/lexical_cast.hpp>

extern "C" {
#include <sys/socket.h>
#include <string.h>
}

using namespace lev;
namespace tcp_proxy
{
   bool debug = true;
   class bridge : public boost::enable_shared_from_this<bridge>
   {
   public:
      static std::multimap<IpAddr, boost::shared_ptr<tcp_proxy::bridge>, IpAddrCompare> ssplice_pending_bridge_ptrs_;
      typedef boost::shared_ptr<bridge> ptr_type;
      typedef boost::weak_ptr<bridge> weak_bridge_ptr_type;
      weak_bridge_ptr_type wbp_;
      static unsigned long num_upstream_connections_;
      static unsigned long num_downstream_connections_;

      bridge(struct event_base* evbase, struct evconnlistener* listener,
             evutil_socket_t localhost_fd, IpAddr localhost_address, IpAddr upstream_server)
         : upstream_server_(upstream_server),
           localhost_address_(localhost_address),
           evbase_(evbase),
           upstream_evbuf_(NULL),
           downstream_evbuf_(NULL),
           evlis_(listener),
           localhost_fd_(localhost_fd),
           upstream_bytes_read_(0),
           downstream_bytes_read_(0)
         {
            if(debug) {
               std::cout << "Bridge: "<< this << "localhost fd = " << localhost_fd_ << std::endl;
               this->num_downstream_connections_++;
               sockaddr loc_sock, rem_sock;
               socklen_t len = sizeof(struct sockaddr_in);
               getpeername(localhost_fd, &rem_sock, &len);
               getsockname(localhost_fd, &loc_sock, &len);
               IpAddr loc_ep(loc_sock), rem_ep(rem_sock);
               std::cout << __FUNCTION__ << ": num_downstream_connections = " << num_downstream_connections_ << " " << rem_ep.toStringFull() << "<-->" << loc_ep.toStringFull() << " " << std::endl;
            }
         }

      ~bridge()
         {
            if(debug)
               std::cout << "In bridge destructor " << std::endl;
            //stop();
         }

      void close_upstream()
         {
            // Close the upstream connection
            if(debug) {
               std::cout << "In close_upstream for bridge " << this << std::endl;
            }
            //upstream_evbuf_.own(true);
            //upstream_evbuf_.free();
            bufferevent_free(upstream_evbuf_);
            num_upstream_connections_--;
            if(debug) {
               std::cout << __FUNCTION__ << ":num_upstream_connections = " << num_upstream_connections_ << std::endl;
            }
         }

      void close_downstream()
         {
            // Close the upstream connection
            if(debug) {
               std::cout << "In close_downstream for bridge " << this << std::endl;
            }
            //downstream_evbuf_.own(true);
            //downstream_evbuf_.free();
            bufferevent_free(downstream_evbuf_);
            num_downstream_connections_--;
            if(debug) {
               std::cout << __FUNCTION__ << ": num_downstream_connections = " << num_downstream_connections_ << std::endl;
            }
         }

      static void on_downstream_read(struct bufferevent* bev, void* cbarg)
         {
            if(debug)
               std::cout << "In downstream read ";
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            // if(debug)
            //    std::cout << "for connection " << bridge_inst->localhost_address_.toStringFull() << "<->"<< bridge_inst->upstream_server_.toStringFull() << std::endl;

            // Copy all the data from the input buffer to the output buffer.
            // if(debug) {
            //    std::cout << "Copying buffer of length "  << bridge_inst->downstream_evbuf_.input().length() << "from downstream to upstream " << std::endl;
            // }
            //bridge_inst->upstream_evbuf_.output().append(bridge_inst->downstream_evbuf_.input());
            // evbuffer_add_buffer(bufferevent_get_output(bridge_inst->upstream_evbuf_.get_mPtr()),
            //                     bufferevent_get_input(bridge_inst->downstream_evbuf_.get_mPtr()));
            evbuffer_add_buffer(bufferevent_get_output(bridge_inst->upstream_evbuf_),
                                bufferevent_get_input(bridge_inst->downstream_evbuf_));
            //bridge_inst->downstream_evbuf_.disable(EV_READ);
            bufferevent_disable(bridge_inst->upstream_evbuf_, EV_READ);
         }

      static void on_downstream_write(struct bufferevent* bev, void* cbarg)
         {
            if(debug)
               std::cout << "In downstream write " << std::endl;
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            //bridge_inst->upstream_evbuf_.enable(EV_READ);
            bufferevent_enable(bridge_inst->upstream_evbuf_, EV_READ);
         }

      static void on_downstream_event(struct bufferevent* bev, short events, void* cbarg)
         {
            //EvBufferEvent evbuf(bev);
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            if (events & BEV_EVENT_ERROR)
            {
               std::cerr << "Error: Downstream connection error" << std::endl;
               // // Close the downstream connection
               // evbuf.own(true);
               // evbuf.free();
               //bridge_inst->close_downstream();
               // Close the upstream connection
               // bridge_inst->upstream_evbuf_.own(true);
               // bridge_inst->upstream_evbuf_.free();
               //bridge_inst->close_upstream();
               bridge_inst->stop();
            } else if (events & BEV_EVENT_EOF) {
               std::cerr << "Downstream connection EOF" << std::endl;
               // Close the downstream connection
               // evbuf.own(true);
               // evbuf.free();
               // bridge_inst->close_downstream();
               // Close the upstream connection
               // bridge_inst->upstream_evbuf_.own(true);
               // bridge_inst->upstream_evbuf_.free();
               // bridge_inst->close_upstream();
               bridge_inst->stop();
            } else if (events & BEV_EVENT_TIMEOUT) {
               std::cerr << "Error: Downstream connection TIMEDOUT" << std::endl;
               // Close the downstream connection
               // evbuf.own(true);
               // evbuf.free();
               // bridge_inst->close_downstream();
               // Close the upstream connection
               // bridge_inst->upstream_evbuf_.own(true);
               // bridge_inst->upstream_evbuf_.free();
               // bridge_inst->close_upstream();
               bridge_inst->stop();
            }
         }

      static void on_upstream_read(struct bufferevent* bev, void* cbarg)
         {
            if(debug)
               std::cout << "In upstream read " << std::endl;
            bridge* bridge_inst = static_cast<bridge *>(cbarg);

            // Copy all the data from the input buffer to the output buffer.
            // if(debug) {
            //    std::cout << "Copying buffer of length " << bridge_inst->upstream_evbuf_.input().length() << "from upstream to downstream " << std::endl;
            // }
            //bridge_inst->downstream_evbuf_.output().append(bridge_inst->upstream_evbuf_.input());
            // evbuffer_add_buffer(bufferevent_get_output(bridge_inst->downstream_evbuf_.get_mPtr()),
            //                     bufferevent_get_input(bridge_inst->upstream_evbuf_.get_mPtr()));
            evbuffer_add_buffer(bufferevent_get_output(bridge_inst->downstream_evbuf_),
                                bufferevent_get_input(bridge_inst->upstream_evbuf_));
            //bridge_inst->upstream_evbuf_.disable(EV_READ);
            bufferevent_disable(bridge_inst->upstream_evbuf_, EV_READ);
         }

      static void on_upstream_write(struct bufferevent* bev, void* cbarg)
         {
            if(debug)
               std::cout << "In upstream write " << std::endl;
            bridge* bridge_inst = static_cast<bridge *>(cbarg);
            //bridge_inst->downstream_evbuf_.enable(EV_READ);
            bufferevent_enable(bridge_inst->downstream_evbuf_, EV_READ);
         }

      static void on_upstream_event(struct bufferevent* bev, short events, void* cbarg)
         {
            //EvBufferEvent evbuf(bev);
            sockaddr rem_sock, loc_sock;
            socklen_t len = sizeof(struct sockaddr_in);
            int ret = getpeername(bufferevent_getfd(bev), &rem_sock, &len);
            std::cout << "upstream event for fd" << bufferevent_getfd(bev) << " ; events = " << events << std::endl;
            if(ret == 0) {
               ret = getsockname(bufferevent_getfd(bev), &loc_sock, &len);
               if(ret == 0) {
                  IpAddr remote_server(rem_sock);
                  IpAddr local_server(loc_sock);
                  auto bridge_inst_it = ssplice_pending_bridge_ptrs_.find(rem_sock);
                  if(bridge_inst_it == ssplice_pending_bridge_ptrs_.end()) {
                     std::cerr << "Could not find a bridge for upstream_server " << remote_server.toStringFull() << std::endl;
                     exit(1);
                  }

                  ptr_type bridge_inst = bridge_inst_it->second;
                  // bridge* b = static_cast<bridge *>(cbarg);
                  // ptr_type bridge_inst = boost::shared_ptr<bridge>(b);
                  ssplice_pending_bridge_ptrs_.erase(bridge_inst_it);
                  if(debug)
                     std::cout << __FUNCTION__ << ":ssplice_pending_bridge_ptrs.size() = " << ssplice_pending_bridge_ptrs_.size() << std::endl;
                  if (events & BEV_EVENT_CONNECTED)
                  {
                     num_upstream_connections_++;
                     std::cout << "US Conn. " << num_upstream_connections_ << " - Connected to upstream (" << local_server.toStringFull() << "<-->" << remote_server.toStringFull() << ")" << std::endl;
                     if(debug)
                        std::cout << "; upstream fd= " << bufferevent_getfd(bev) << "; bridge ptr: "<< bridge_inst.get() << std::endl;
                     //evbuf.setTcpNoDelay();
                     int one = 1;
                     setsockopt(bufferevent_getfd(bev), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                     //set the call backs for downstream and upstream
                     // if (bridge_inst->downstream_evbuf_.newForSocket(bridge_inst->localhost_fd_, on_downstream_read, on_downstream_write,
                     //                                                 on_downstream_event, (void *)bridge_inst.get(), bridge_inst->evbase_->base()))
                     // {
                     //    bridge_inst->downstream_evbuf_.enable(EV_READ | EV_WRITE);
                     //    bridge_inst->downstream_evbuf_.setTcpNoDelay();
                     //    bridge_inst->downstream_evbuf_.setTcpKeepAlive();
                     //    bridge_inst->downstream_evbuf_.own(false);
                     //    if(debug) {
                     //       std::cout << "Enabled downstream_evbuf ";
                     //       std::cout << "; downstream fd = " << bridge_inst->downstream_evbuf_.getBufEventFd() << std::endl;
                     //    }
                     // }

                     bridge_inst->downstream_evbuf_ = bufferevent_socket_new(bridge_inst->evbase_, bridge_inst->localhost_fd_, BEV_OPT_CLOSE_ON_FREE);
                     if (bridge_inst->downstream_evbuf_ == NULL)
                     {
                        std::cerr <<"Failed to create libevent buffer event" << std::endl;
                        return;
                     }

                     bufferevent_setcb(bridge_inst->downstream_evbuf_, on_downstream_read, on_downstream_write,
                                       on_downstream_event, (void *)bridge_inst.get());
                     //bufferevent_enable(bridge_inst->downstream_evbuf_, EV_READ | EV_WRITE);
                     bufferevent_enable(bridge_inst->downstream_evbuf_, EV_READ);
                     bufferevent_enable(bridge_inst->downstream_evbuf_, EV_WRITE);
                     one = 1;
                     setsockopt(bufferevent_getfd(bridge_inst->downstream_evbuf_), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                     setsockopt(bufferevent_getfd(bridge_inst->downstream_evbuf_), SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
                     if(debug) {
                        std::cout << "Enabled downstream_evbuf ";
                        std::cout << "; downstream fd = " << bufferevent_getfd(bridge_inst->downstream_evbuf_) << std::endl;
                     }

                     // if (bridge_inst->downstream_evbuf_.newForSocket(bridge_inst->localhost_fd_, on_downstream_read, on_downstream_write,
                     //                                                 on_downstream_event, (void *)bridge_inst.get(), bridge_inst->evbase_->base()))
                     // {
                     //    bridge_inst->downstream_evbuf_.enable(EV_READ | EV_WRITE);
                     //    bridge_inst->downstream_evbuf_.setTcpNoDelay();
                     //    bridge_inst->downstream_evbuf_.setTcpKeepAlive();
                     //    bridge_inst->downstream_evbuf_.own(false);
                     //    if(debug) {
                     //       std::cout << "Enabled downstream_evbuf ";
                     //       std::cout << "; downstream fd = " << bridge_inst->downstream_evbuf_.getBufEventFd() << std::endl;
                     //    }
                     // }

                     // bridge_inst->upstream_evbuf_.set_cb(on_upstream_read, on_upstream_write, on_upstream_event, (void*)bridge_inst.get());
                     // bridge_inst->upstream_evbuf_.enable(EV_READ);
                     // bridge_inst->upstream_evbuf_.enable(EV_WRITE);
                     // bridge_inst->upstream_evbuf_.setTcpNoDelay();
                     // bridge_inst->upstream_evbuf_.setTcpKeepAlive();
                     // bridge_inst->upstream_evbuf_.own(false);
                     bufferevent_setcb(bridge_inst->upstream_evbuf_, on_upstream_read, on_upstream_write,
                                       on_upstream_event, (void *)bridge_inst.get());
                     //bufferevent_enable(bridge_inst->upstream_evbuf_, EV_READ | EV_WRITE);
                     bufferevent_enable(bridge_inst->upstream_evbuf_, EV_READ);
                     bufferevent_enable(bridge_inst->upstream_evbuf_, EV_WRITE);
                     one = 1;
                     setsockopt(bufferevent_getfd(bridge_inst->upstream_evbuf_), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                     setsockopt(bufferevent_getfd(bridge_inst->upstream_evbuf_), SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
                     if(debug)
                        std::cout << "Enabled upstream_evbuf and reset its callbacks" << std::endl;
                  } else if (events & BEV_EVENT_ERROR) {
                     std::cout << "Error: Upstream connection to " << bridge_inst->upstream_server_.toStringFull() << " failed" << std::endl;
                     // Close the upstream connection
                     // evbuf.own(true);
                     // evbuf.free();
                     // bridge_inst->close_upstream();
                     // Close the downstream connection
                     // bridge_inst->downstream_evbuf_.own(true);
                     // bridge_inst->downstream_evbuf_.free();
                     // bridge_inst->close_downstream();
                     bridge_inst->stop();
                  } else if (events & BEV_EVENT_TIMEOUT) {
                     std::cerr << "Error: Upstream connection to " << bridge_inst->upstream_server_.toStringFull() << "TIMEDOUT" << std::endl;
                     // Close the upstream connection
                     // evbuf.own(true);
                     // evbuf.free();
                     // bridge_inst->close_upstream();
                     // Close the downstream connection
                     // bridge_inst->downstream_evbuf_.own(true);
                     // bridge_inst->downstream_evbuf_.free();
                     // bridge_inst->close_downstream();
                     bridge_inst->stop();
                  }
               } else {
                  std::cout << strerror(errno) << "events = " << events << std::endl;
               }
            } else {
               std::cout << strerror(errno) << "events = " << events << std::endl;
            }
         }

      void stop() {
         close_upstream();
         close_downstream();
         // Unref the current bridge instance from global list of bridge instances
         for(auto it = tcp_proxy::bridge::acceptor::bridge_instances_.begin() ;
             it < tcp_proxy::bridge::acceptor::bridge_instances_.end(); it++) {
            // found nth element..print and break.
            if((*it).get() == this) {
               std::cout << "Unrefing bridge @ " << this << "from global bridge instance list "<< std::endl;
               tcp_proxy::bridge::acceptor::bridge_instances_.erase(it);
               break;
            }
         }
      }

      void start()
         {
            ptr_type p = wbp_.lock();
            if(!p) {
               std::cerr << "Error: Could not instantiate shared ptr for bridge" << std::endl;
               stop();
            } else {
               ssplice_pending_bridge_ptrs_.insert(std::pair<IpAddr, ptr_type> (upstream_server_, p));
               if(debug)
                  std::cout << __FUNCTION__ << ":ssplice_pending_bridge_ptrs.size() = " << ssplice_pending_bridge_ptrs_.size() << std::endl;
               upstream_evbuf_ = bufferevent_socket_new(evbase_, -1, BEV_OPT_CLOSE_ON_FREE);
               if (upstream_evbuf_ == NULL)
               {
                  std::cerr <<"Failed to create libevent buffer event" << std::endl;
                  return;
               }

               bufferevent_setcb(upstream_evbuf_, on_upstream_read, on_upstream_write,
                                 on_upstream_event, (void*)this);

               //bufferevent_disable(upstream_evbuf_, EV_READ | EV_WRITE);
               if(debug) {
                  //std::cout << "Created upstream_eventbuf_ (evlistener = )" << upstream_evbuf_.get_mPtr() << ") ";
                  std::cout << "Created upstream_eventbuf_ (evlistener = )" << upstream_evbuf_ << ") ";
                  std::cout << "for connection " << localhost_address_.toStringFull() << "<->"<< upstream_server_.toStringFull();
                  std::cout << "; bridge ptr= " << this << std::endl;
               }
               // if (upstream_evbuf_.newForSocket(-1, on_upstream_read, on_upstream_write,
               //                                  on_upstream_event, (void*)this, evbase_->base()))
               // {
               //    if(debug) {
               //       std::cout << "Created upstream_eventbuf_ (evlistener = )" << upstream_evbuf_.get_mPtr() << ") ";
               //       std::cout << "for connection " << localhost_address_.toStringFull() << "<->"<< upstream_server_.toStringFull();
               //       std::cout << "; bridge ptr= " << this << std::endl;
               //    }
               //    upstream_evbuf_.disable(EV_READ);
               //    upstream_evbuf_.disable(EV_WRITE);
               // }

               // Connect
               // if (!upstream_evbuf_.connect(upstream_server_))
               if (bufferevent_socket_connect(upstream_evbuf_, (sockaddr*)upstream_server_.addr(), upstream_server_.addrLen()) != 0)
               {
                  std::cerr << "Error: Client failed to connect to " << upstream_server_.toStringFull() << std::endl;
               } else {
                  if(debug)
                     std::cout << "Inititated connection " << localhost_address_.toStringFull() << "<->"<< upstream_server_.toStringFull() << std::endl;
               }
            }
         }

   private:
      IpAddr upstream_server_;
      IpAddr localhost_address_;
      //EvBaseLoop* evbase_;
      struct event_base* evbase_;
      //EvBufferEvent upstream_evbuf_, downstream_evbuf_;
      struct bufferevent *upstream_evbuf_, *downstream_evbuf_;
      //EvConnListener evlis;
      struct evconnlistener* evlis_;
      evutil_socket_t localhost_fd_;
      int64_t upstream_bytes_read_, downstream_bytes_read_;
   public:

      class acceptor
      {
      public:
         static std::vector<ptr_type> bridge_instances_;
         acceptor(struct event_base* evbase, const std::string& local_host, unsigned short local_port,
                  const std::string& upstream_host, unsigned short upstream_port)
            : evbase_(evbase), upstream_server_(upstream_host.c_str(), upstream_port),
              localhost_address_(local_host.c_str(), local_port), listener_(NULL)
            {}

         ~acceptor()
            {
               if(debug)
                  std::cout << "In acceptor destructor " << std::endl;
            }
         bool accept_connections()
            {
               try
               {
                  std::cout << "Waiting to accept connections" << std::endl << std::endl;
                  // listener_.newListener(localhost_address_, onAccept,
                  //                       (void *)this, evbase_->base());
                  listener_ = evconnlistener_new_bind(evbase_, onAccept, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                                      localhost_address_.addr(), localhost_address_.addrLen());
                  if(!listener_) {
                     std::cerr << "acceptor exception: " << std::endl;
                     return false;
                  }
                  //evbase_->loop();
                  event_base_loop(evbase_, 0);
               } catch(std::exception& e) {
                  std::cerr << "acceptor exception: " << e.what() << std::endl;
                  return false;
               }
               return true;
            }
         static void onAccept(struct evconnlistener* listener, evutil_socket_t listener_fd, struct sockaddr* address,
                              int socklen, void* cbarg)
            {
               // sockaddr loc_sock, rem_sock;
               // socklen_t len = sizeof(struct sockaddr_in);
               // getpeername(listener_fd, &rem_sock, &len);
               // getsockname(listener_fd, &loc_sock, &len);
               // IpAddr loc_ep(loc_sock), rem_ep(rem_sock);

               // //num_downstream_connections_++;
               // if(debug) {
               //    std::cout << "Accepted connection: " << rem_ep.toStringFull() << "<-->" << loc_ep.toStringFull() << " ";
               // }
               acceptor *acceptor_inst = static_cast<acceptor *>(cbarg);
               ptr_type p = boost::shared_ptr<bridge>(new bridge(acceptor_inst->evbase_, listener, listener_fd,
                                                                 acceptor_inst->localhost_address_,
                                                                 acceptor_inst->upstream_server_));
               p->wbp_ = p;
               bridge_instances_.push_back(p);
               if(debug)
                  std::cout << " ; loc fd = " << listener_fd << "; bridge ptr = " << p.get() << std::endl;
               p->start();
            }
      private:
         //ptr_type bridge_session_;
         //EvBaseLoop* evbase_;
         struct event_base* evbase_;
         IpAddr upstream_server_;
         IpAddr localhost_address_;
         //EvConnListener listener_;
         struct evconnlistener* listener_;
      };
   };
}

std::multimap<IpAddr, boost::shared_ptr<tcp_proxy::bridge>, IpAddrCompare> tcp_proxy::bridge::ssplice_pending_bridge_ptrs_;
std::vector<boost::shared_ptr<tcp_proxy::bridge> > tcp_proxy::bridge::acceptor::bridge_instances_;
unsigned long tcp_proxy::bridge::num_downstream_connections_ = 0;
unsigned long tcp_proxy::bridge::num_upstream_connections_ = 0;

void onCtrlC(evutil_socket_t fd, short what, void* arg)
{
   EvEvent* ev = (EvEvent*)arg;
   std::cout << "Ctrl-C --exiting loop" << std::endl;
   // Destroy all the bridge instances
   tcp_proxy::bridge::acceptor::bridge_instances_.erase(tcp_proxy::bridge::acceptor::bridge_instances_.begin(),
                                                        tcp_proxy::bridge::acceptor::bridge_instances_.end());
   tcp_proxy::bridge::ssplice_pending_bridge_ptrs_.erase(tcp_proxy::bridge::ssplice_pending_bridge_ptrs_.begin(),
                                                         tcp_proxy::bridge::ssplice_pending_bridge_ptrs_.end());
   ev->exitLoop();
}

int main(int argc, char* argv[])
{
   if (argc != 6)
   {
      std::cerr << "usage: tcpproxy <local host ip> <local port> <forward host ip> <forward port> <debug-true/false>" << std::endl;
      return 1;
   }
   //EvBaseLoop evbase;
   struct event_base* evbase = event_base_new();
   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];
   debug = boost::lexical_cast<bool>(argv[5]);

   signal(SIGPIPE, SIG_IGN);
   //EvEvent ctrlc;
   struct event *evnt_ctrlc = event_new(evbase, SIGINT, EV_PERSIST | EV_SIGNAL, onCtrlC, NULL);
   event_add(evnt_ctrlc, NULL);
   //ctrlc.newSignal(onCtrlC, SIGINT, evbase);
   //ctrlc.start();

   // EvEvent evstop;
   // evstop.newSignal(onCtrlC, SIGHUP, evbase);
   // evstop.start();
   struct event *evnt_stop = event_new(evbase, SIGHUP, EV_PERSIST | EV_SIGNAL, onCtrlC, NULL);
   event_add(evnt_stop, NULL);

   try
   {
      tcp_proxy::bridge::acceptor acceptor(evbase,
                                           local_host, local_port,
                                           forward_host, forward_port);
      std::cout << "Created acceptor object @ " << &acceptor << std::endl;
      acceptor.accept_connections();
   } catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      event_base_free(evbase);
      return 1;
   }
   event_base_free(evbase);
}

/*
 * [Note] On posix systems the tcp proxy server build command is as follows:
 * c++ -pedantic -ansi -Wall -Werror -O3 -o tcpproxy_server tcpproxy_server.cpp -L/usr/lib -lstdc++ -lpthread -lboost_thread -lboost_system
 */
