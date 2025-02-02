#include "http_server.h"
#include "sylar/log.h"
#include "sylar/http/servlets/config_servlet.h"
#include "sylar/http/servlets/metrics_servlet.h"
#ifdef SYLAR_ENABLE_PROFILER
#include "sylar/http/servlets/profiler_servlet.h"
#endif
#include "sylar/http/servlets/status_servlet.h"

namespace sylar {
namespace http {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive
               ,sylar::IOManager* worker
               ,sylar::IOManager* io_worker
               ,sylar::IOManager* accept_worker)
    :TcpServer(worker, io_worker, accept_worker)
    ,m_isKeepalive(keepalive) {
    m_dispatch = std::make_shared<ServletDispatch>();

    m_type = "http";
    m_dispatch->addServlet("/_/status", std::make_shared<StatusServlet>());
    m_dispatch->addServlet("/_/config", std::make_shared<ConfigServlet>());
    m_dispatch->addServlet("/metrics", std::make_shared<MetricsServlet>());
#ifdef SYLAR_ENABLE_PROFILER
    m_dispatch->addGlobServlet("/profiler/*", std::make_shared<ProfilerServlet>());
#endif
}

void HttpServer::setName(const std::string& v) {
    TcpServer::setName(v);
    m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
    SYLAR_LOG_DEBUG(g_logger) << "handleClient " << *client;
    //sylar::TimeCalc tc;
    HttpSession::ptr session = std::make_shared<HttpSession>(client);
    do {
        auto req = session->recvRequest();
        //tc.tick("recv");
        if(!req) {
            SYLAR_LOG_DEBUG(g_logger) << "recv http request fail, errno="
                << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }

        HttpResponse::ptr rsp = std::make_shared<HttpResponse>(req->getVersion()
                            ,req->isClose() || !m_isKeepalive);
        rsp->setHeader("Server", getName());
        rsp->setHeader("Content-Type", "application/json;charset=utf8");
        {
            sylar::SchedulerSwitcher sw(m_worker);
            m_dispatch->handle(req, rsp, session);
        }
        //tc.tick("handler");
        session->sendResponse(rsp);
        //tc.tick("response");
        //SYLAR_LOG_ERROR(g_logger) << "elapse=" << tc.elapse() << " - " << tc.toString();

        if(!m_isKeepalive || req->isClose()) {
            break;
        }
    } while(true);
    session->close();
}

}
}
