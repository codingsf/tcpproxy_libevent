#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>
#include "./lev-master/include/lev.h"

using namespace lev;
namespace tcp_proxy
{
   class bridge : public boost::enable_shared_from_this<bridge>
   {
   public:
      typedef boost::shared_ptr<bridge> ptr_type;

      bridge(EvBaseLoop* evbase, struct evconnlistener* listener,
             evutil_socket_t localhost_fd, IpAddr localhost_address, IpAddr upstream_server)
         : upstream_server_(upstream_server),
           localhost_address_(localhost_address),
           evbase_(evbase), evlis(listener),
           localhost_fd_(localhost_fd),
           upstream_bytes_read_(0),
           downstream_bytes_read_(0)
         {}

      static void on_downstream_read(struct bufferevent* bev, void* cbarg)
         {
            std::cout << "In downstream read ";
            EvBufferEvent evbuf(bev);
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            std::cout << "for connection " << bridge_inst->localhost_address_.toStringFull() << "<->"<< bridge_inst->upstream_server_.toStringFull() << std::endl;

            // Copy all the data from the input buffer to the output buffer.
            std::cout << "Copying buffer of length "  << evbuf.input().length() << "from downstream to upstream " << std::endl;
            std::cout << "Downstream evbuf length = "  << bridge_inst->downstream_evbuf_.input().length() << std::endl;
            bridge_inst->upstream_evbuf_.output().append(evbuf.input());
            bridge_inst->downstream_evbuf_.disable(EV_READ);
         }

      static void on_downstream_write(struct bufferevent* bev, void* cbarg)
         {
            std::cout << "In downstream write " << std::endl;
            EvBufferEvent evbuf(bev);
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            bridge_inst->upstream_evbuf_.enable(EV_READ);
         }

      static void on_downstream_event(struct bufferevent* bev, short events, void* cbarg)
         {
            EvBufferEvent evbuf(bev);
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            if (events & BEV_EVENT_ERROR)
            {
               std::cerr << "Error: Downstream connection error" << std::endl;
               // Close the downstream connection
               evbuf.own(true);
               evbuf.free();
               // Close the upstream connection
               bridge_inst->upstream_evbuf_.own(true);
               bridge_inst->upstream_evbuf_.free();
            } else if (events & BEV_EVENT_EOF) {
               std::cerr << "Downstream connection EOF" << std::endl;
               // Close the downstream connection
               evbuf.own(true);
               evbuf.free();
               // Close the upstream connection
               bridge_inst->upstream_evbuf_.own(true);
               bridge_inst->upstream_evbuf_.free();
            } else if (events & BEV_EVENT_TIMEOUT) {
               std::cerr << "Error: Downstream connection TIMEDOUT" << std::endl;
               // Close the downstream connection
               evbuf.own(true);
               evbuf.free();
               // Close the upstream connection
               bridge_inst->upstream_evbuf_.own(true);
               bridge_inst->upstream_evbuf_.free();
            }
         }

      static void on_upstream_read(struct bufferevent* bev, void* cbarg)
         {
            std::cout << "In upstream read " << std::endl;
            EvBufferEvent evbuf(bev);
            bridge* bridge_inst = static_cast<bridge *>(cbarg);

            // Copy all the data from the input buffer to the output buffer.
            std::cout << "Copying buffer of length " << evbuf.input().length() << "from upstream to downstream " << std::endl;
            std::cout << "Upstream evbuf length = "  << bridge_inst->upstream_evbuf_.input().length() << std::endl;
            bridge_inst->downstream_evbuf_.output().append(evbuf.input());
            bridge_inst->upstream_evbuf_.disable(EV_READ);
         }

      static void on_upstream_write(struct bufferevent* bev, void* cbarg)
         {
            std::cout << "In upstream write " << std::endl;
            EvBufferEvent evbuf(bev);
            bridge* bridge_inst = static_cast<bridge *>(cbarg);

            bridge_inst->downstream_evbuf_.enable(EV_READ);
         }

      static void on_upstream_event(struct bufferevent* bev, short events, void* cbarg)
         {
            EvBufferEvent evbuf(bev);
            bridge *bridge_inst = static_cast<bridge *>(cbarg);

            if (events & BEV_EVENT_CONNECTED)
            {
               std::cout << "Connected to upstream (" << bridge_inst->upstream_server_.toStringFull() << ")" << std::endl;
               evbuf.setTcpNoDelay();
               //set the call backs for downstream and upstream
               if (bridge_inst->downstream_evbuf_.newForSocket(bridge_inst->localhost_fd_, on_downstream_read, on_downstream_write,
                                                               on_downstream_event, (void *)bridge_inst, bridge_inst->evbase_->base()))
               {
                  bridge_inst->downstream_evbuf_.enable(EV_READ | EV_WRITE);
                  bridge_inst->downstream_evbuf_.setTcpNoDelay();
                  bridge_inst->downstream_evbuf_.own(false);
                  std::cout << "Enabled downstream_evbuf " << std::endl;
               }
               if (bridge_inst->upstream_evbuf_.newForSocket(bridge_inst->upstream_evbuf_.getBufEventFd(), on_upstream_read, on_upstream_write,
                                                               on_upstream_event, (void *)bridge_inst, bridge_inst->evbase_->base()))
               {
                  bridge_inst->upstream_evbuf_.enable(EV_READ | EV_WRITE);
                  bridge_inst->upstream_evbuf_.setTcpNoDelay();
                  bridge_inst->upstream_evbuf_.own(false);
                  std::cout << "Enabled upstream_evbuf " << std::endl;
               }

            } else if (events & BEV_EVENT_ERROR) {
               std::cout << "Error: Upstream connection to " << bridge_inst->upstream_server_.toStringFull() << " failed" << std::endl;
               // Close the upstream connection
               evbuf.own(true);
               evbuf.free();
               // Close the downstream connection
               bridge_inst->downstream_evbuf_.own(true);
               bridge_inst->downstream_evbuf_.free();
            } else if (events & BEV_EVENT_TIMEOUT) {
               std::cerr << "Error: Upstream connection to " << bridge_inst->upstream_server_.toStringFull() << "TIMEDOUT" << std::endl;
               // Close the upstream connection
               evbuf.own(true);
               evbuf.free();
               // Close the downstream connection
               bridge_inst->downstream_evbuf_.own(true);
               bridge_inst->downstream_evbuf_.free();
            }
         }

      void start()
         {
            if (upstream_evbuf_.newForSocket(-1, on_upstream_read, on_upstream_write,
                                             on_upstream_event, (void*)this, evbase_->base()))
            {
               std::cout << "Created upstream_eventbuf_ for connection " << localhost_address_.toStringFull() << "<->"<< upstream_server_.toStringFull() << std::endl;
               upstream_evbuf_.enable(EV_READ | EV_WRITE);
            }

            // Connect
            if (!upstream_evbuf_.connect(upstream_server_))
            {
               std::cerr << "Error: Client failed to connect to " << upstream_server_.toStringFull() << std::endl;
            } else {
               std::cout << "Inititated connection " << localhost_address_.toStringFull() << "<->"<< upstream_server_.toStringFull() << std::endl;
            }
         }

   private:
      IpAddr upstream_server_;
      IpAddr localhost_address_;
      EvBaseLoop* evbase_;
      EvBufferEvent upstream_evbuf_, downstream_evbuf_;
      EvConnListener evlis;
      evutil_socket_t localhost_fd_;
      int64_t upstream_bytes_read_, downstream_bytes_read_;

   public:

      class acceptor
      {
      public:
         acceptor(EvBaseLoop* evbase, const std::string& local_host, unsigned short local_port,
                  const std::string& upstream_host, unsigned short upstream_port)
            : evbase_(evbase), upstream_server_(upstream_host.c_str(), upstream_port),
              localhost_address_(local_host.c_str(), local_port)
            {}

         bool accept_connections()
            {
               try
               {
                  std::cout << "Waiting to accept connections" << std::endl;
                  listener_.newListener(localhost_address_, onAccept,
                                        (void *)this, evbase_->base());
                  evbase_->loop();
               } catch(std::exception& e) {
                  std::cerr << "acceptor exception: " << e.what() << std::endl;
                  return false;
               }
               return true;
            }
         static void onAccept(struct evconnlistener* listener, evutil_socket_t listener_fd, struct sockaddr* address,
                              int socklen, void* cbarg)
            {
               std::cout << __FUNCTION__ << "Asynchronously accepted connections" << std::endl;
               acceptor *acceptor_inst = static_cast<acceptor *>(cbarg);
               acceptor_inst->bridge_session_ = boost::shared_ptr<bridge>(new bridge(acceptor_inst->evbase_, listener, listener_fd,
                                                                                     acceptor_inst->localhost_address_,
                                                                                     acceptor_inst->upstream_server_));
               acceptor_inst->bridge_session_->start();
            }
      private:
         ptr_type bridge_session_;
         EvBaseLoop* evbase_;
         IpAddr upstream_server_;
         IpAddr localhost_address_;
         EvConnListener listener_;
      };
   };
}

void onCtrlC(evutil_socket_t fd, short what, void* arg)
{
   EvEvent* ev = (EvEvent*)arg;
   printf("Ctrl-C --exiting loop\n");
   ev->exitLoop();
}

int main(int argc, char* argv[])
{
   if (argc != 5)
   {
      std::cerr << "usage: tcpproxy <local host ip> <local port> <forward host ip> <forward port>" << std::endl;
      return 1;
   }
   EvBaseLoop evbase;
   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];

   signal(SIGPIPE, SIG_IGN);
   EvEvent ctrlc;
   ctrlc.newSignal(onCtrlC, SIGINT, evbase);
   ctrlc.start();

   EvEvent evstop;
   evstop.newSignal(onCtrlC, SIGHUP, evbase);
   evstop.start();

   try
   {
      tcp_proxy::bridge::acceptor acceptor(&evbase,
                                           local_host, local_port,
                                           forward_host, forward_port);
      std::cout << "Created acceptor object" << std::endl;
      acceptor.accept_connections();
   } catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }
}

/*
 * [Note] On posix systems the tcp proxy server build command is as follows:
 * c++ -pedantic -ansi -Wall -Werror -O3 -o tcpproxy_server tcpproxy_server.cpp -L/usr/lib -lstdc++ -lpthread -lboost_thread -lboost_system
 */
