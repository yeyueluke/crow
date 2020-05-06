#include "crow.h"
#include <string>
#include <vector>
#include <chrono>
#include <regex>
#include <boost/filesystem.hpp>
using namespace boost;

using namespace std;

vector<string> msgs;
vector<pair<crow::response*, decltype(chrono::steady_clock::now())>> ress;

static void writeResponse(crow::response& resp, int code, const std::string& message)
{
    resp = crow::response(code);
    std::ostringstream os;
    os << message.length();
    resp.set_header("Content-Length", os.str());
    resp.end(message);
}
static void responseStaticResource(const crow::request& req, crow::response& resp, const std::string &webServerPath, std::string path)
{
	auto iter = req.headers.find("Origin");
	if (iter != req.headers.end()) {
		resp.set_header("Access-Control-Allow-Origin", iter->second);
	}

	std::smatch match;
	if (std::regex_match(path, match, std::regex("^.+\\.([A-Za-z][A-Za-z0-9]*)$"))) {
		std::string fileType = match[1];
		std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);
		std::string contentType = "text/plain";
		if ("html" == fileType || "htm" == fileType) {
			contentType = "text/html";
		} else if ("js" == fileType) {
			contentType = "application/javascript";
		} else if ("css" == fileType) {
			contentType = "text/css";
		} else if ("gif" == fileType) {
			contentType = "image/gif";
		} else if ("png" == fileType) {
			contentType = "image/png";
		} else if ("jpg" == fileType) {
			contentType = "image/jpeg";
		} else if ("ico" == fileType) {
			contentType = "image/x-icon";
		}

		std::string fileName = webServerPath + "webapp/" + path;
        if (filesystem::exists(filesystem::path(fileName + ".gz")))
        {
            fileName += ".gz";
            resp.set_header("Content-Encoding", "gzip");
        }

		std::ifstream in(fileName);
		if (in.is_open()) {
			std::string buffer{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
			resp.set_header("Content-Type", contentType);
			std::ostringstream os;
			os << buffer.length();
			resp.set_header("Content-Length", os.str());
			resp.write(buffer);
			resp.end();
			return;
		} 
	}

	std::string error("Not found /" + path);
	writeResponse(resp, 404, error);
}

void broadcast(const string& msg)
{
    msgs.push_back(msg);
    crow::json::wvalue x;
    x["msgs"][0] = msgs.back();
    x["last"] = msgs.size();
    string body = crow::json::dump(x);
    for(auto p : ress)
    {
        auto* res = p.first;
        CROW_LOG_DEBUG << res << " replied: " << body;
        res->end(body);
    }
    ress.clear();
}
// To see how it works go on {ip}:40080 but I just got it working with external build (not directly in IDE, I guess a problem with dependency)
int main()
{
    string webServerPath = "/path/to/html_dir/";
    crow::SimpleApp app;
    crow::mustache::set_base(".");

    CROW_ROUTE(app, "/")
    ([]{
        crow::mustache::context ctx;
        return crow::mustache::load("example_chat.html").render();
    });

    CROW_ROUTE(app, "/logs")
    ([]{
        CROW_LOG_INFO << "logs requested";
        crow::json::wvalue x;
        int start = max(0, (int)msgs.size()-100);
        for(int i = start; i < (int)msgs.size(); i++)
            x["msgs"][i-start] = msgs[i];
        x["last"] = msgs.size();
        CROW_LOG_INFO << "logs completed";
        return x;
    });

    CROW_ROUTE(app, "/logs/<int>")
    ([](const crow::request& /*req*/, crow::response& res, int after){
        CROW_LOG_INFO << "logs with last " << after;
        if (after < (int)msgs.size())
        {
            crow::json::wvalue x;
            for(int i = after; i < (int)msgs.size(); i ++)
                x["msgs"][i-after] = msgs[i];
            x["last"] = msgs.size();

            res.write(crow::json::dump(x));
            res.end();
        }
        else
        {
            vector<pair<crow::response*, decltype(chrono::steady_clock::now())>> filtered;
            for(auto p : ress)
            {
                if (p.first->is_alive() && chrono::steady_clock::now() - p.second < chrono::seconds(30))
                    filtered.push_back(p);
                else
                    p.first->end();
            }
            ress.swap(filtered);
            ress.push_back({&res, chrono::steady_clock::now()});
            CROW_LOG_DEBUG << &res << " stored " << ress.size();
        }
    });

    CROW_ROUTE(app, "/send")
        .methods("GET"_method, "POST"_method)
    ([](const crow::request& req)
    {
        CROW_LOG_INFO << "msg from client: " << req.body;
        broadcast(req.body);
        return "";
    });

    app.route_dynamic("/<path>")
    ([&webServerPath](const crow::request& req, crow::response& resp, std::string path) {
        responseStaticResource(req, resp, webServerPath, path);
    });
    
    app.port(40080)
        //.multithreaded()
        .run();
}
