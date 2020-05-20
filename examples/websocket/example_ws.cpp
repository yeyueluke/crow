#include "crow.h"
#include <unordered_set>
#include <mutex>

#define MYMAP "/home/nvidia/tst/web_tst/crow-master/build/examples/templates/time.jpg"
int main()
{
    crow::SimpleApp app;

    std::mutex mtx;;
    std::unordered_set<crow::websocket::connection*> users;

    CROW_ROUTE(app, "/ws")
        .websocket()
        .onopen([&](crow::websocket::connection& conn){
                CROW_LOG_INFO << "new websocket connection";
                std::lock_guard<std::mutex> _(mtx);
                users.insert(&conn);
                })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason){
                CROW_LOG_INFO << "websocket connection closed: " << reason;
                std::lock_guard<std::mutex> _(mtx);
                users.erase(&conn);
                })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary){
                std::lock_guard<std::mutex> _(mtx);
                for(auto u:users)
                    if (is_binary)
                    {
						//u->send_binary(data);
						struct stat buf;

                        if( stat(MYMAP, &buf ) != -1 )
                            printf( "File size = %d\n", buf.st_size);       
                      
                        fstream f(MYMAP, ios::in | ios::binary);
                        if (f.bad()) 
                            cout << "bad file open" << endl;
                            
                        unsigned char *img = new unsigned char[buf.st_size];
                        f.read((char*)img, buf.st_size);
                        f.close();
                        string imgstr((char *)img,buf.st_size);
                        u->send_binary(imgstr);    

                        /*ofstream origin1("bb.dat");
                        origin1.write((char*)imgstr.data(),imgstr.size());
                        origin1.close();
                        cout << "imgstr.size " << imgstr.size() << endl;
                        //cout << "binary  data " << data << endl;
                        for(int i = 0; i < data.size();i++)
                            printf("0x%x ",data[i]);
                        printf("\n")*/
					}	
                    else
                        u->send_text(data);
                });

    CROW_ROUTE(app, "/")
    ([]{
        char name[256];
        gethostname(name, 256);
        crow::mustache::context x;
        x["servername"] = name;
	
        auto page = crow::mustache::load("ws.html");
        return page.render(x);
     });

    app.port(40080)
        .multithreaded()
        .run();
}
