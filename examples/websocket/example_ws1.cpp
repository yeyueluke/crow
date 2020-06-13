#include "crow.h"
#include <unordered_set>
#include <mutex>

#include <regex>
#include <boost/filesystem.hpp>
#include <iostream>
#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/OccupancyGrid.h>
using namespace std;
using namespace boost;

union FloatLongType
{
	float fdata;
	unsigned long ldata;
};
void Float_to_Byte(float f,unsigned char byte[])
{
	FloatLongType fl;
	fl.fdata = f;
	byte[0] = (unsigned char)fl.ldata;
	byte[1] = (unsigned char)(fl.ldata>>8);
	byte[2] = (unsigned char)(fl.ldata>>16);
	byte[3] = (unsigned char)(fl.ldata>>24);
}

sensor_msgs::LaserScan laser_info;
unsigned char laserscan_data[1440];
void laserCallback(const sensor_msgs::LaserScan::ConstPtr& scan)
{
    laser_info.header = scan->header;
    laser_info.angle_min = scan->angle_min;
    laser_info.angle_max = scan->angle_max;
    laser_info.angle_increment = scan->angle_increment;
    laser_info.time_increment = scan->time_increment;
    laser_info.scan_time = scan->scan_time;
    laser_info.range_min = scan->range_min;
    laser_info.range_max = scan->range_max;

    //laserscan_data  = new unsigned char[scan->ranges.size() * 4]; 
    uint8_t byte[4] = {0};
    static int test = 1;
    if(test)
    {
        memset(laserscan_data,0,sizeof(laserscan_data));
    
        for(int i = 0; i < scan->ranges.size(); i++)
        {
            //printf("%lf \n",scan->ranges[i]);
            Float_to_Byte(scan->ranges[i], byte);
            for(int j = 0; j < 4; j++)
            {
                laserscan_data[i * 4 + j] = byte[j];
                //printf("%x \n",laserscan_data[i * 4 + j]);
            }
            memset(byte,0,sizeof(byte));
            
        }
        /*for(int i = 0; i < 1440; i++)
        {
            printf("%x ",laserscan_data[i]);
            if(i % 32 == 0)
                printf("\n");
        }
        printf("\n");
        test = 0;*/
    }
    //cout <<"ranges.size() " << scan->ranges.size() << endl;
}

static void writeResponse(crow::response& resp, int code, const std::string& message)
{
    resp = crow::response(code);
    std::ostringstream os;
    os << message.length();
    cout << "message " << message << endl;
    resp.set_header("Content-Length", os.str());
    resp.end(message);
}
static void responseStaticResource(const crow::request& req, crow::response& resp, const std::string &webServerPath, std::string path)
{
	auto iter = req.headers.find("Origin");
	if (iter != req.headers.end()) {
		resp.set_header("Access-Control-Allow-Origin", iter->second);
	}
    cout << "path = " << path << endl;
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

		//std::string fileName = webServerPath + "webapp/" + path;
        std::string fileName = webServerPath +  path;
        cout << "fileName = " << fileName << endl;

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
            //cout << "buffer " << buffer << endl;
			resp.end();
			return;
		}
	}

	std::string error("Not found /" + path);
	writeResponse(resp, 404, error);
}
void rosThrd()
{
    ros::spin();
}
unsigned char *mapdata = nullptr;
unsigned int mapSize = 0;
nav_msgs::OccupancyGrid map_info;
void mapCallback(const nav_msgs::OccupancyGrid::ConstPtr& map)
{
    //save mapmetadata info
    map_info.header = map->header;
    map_info.info   = map->info;
    mapSize = map->data.size();
    mapdata = new unsigned char[map->data.size()];
    for(unsigned int y = 0; y < map->info.height; y++) {
        for(unsigned int x = 0; x < map->info.width; x++) {
          unsigned int i = x + (map->info.height - y - 1) * map->info.width;
          cout << "i " << i << endl;
          if (map->data[i] >= 0 && map->data[i] < 40) { //occ [0,0.1)
            mapdata[i] = 254;
          } else if (map->data[i] > +50) { //occ (0.65,1]
            mapdata[i] =000;
          } else { //occ [0.1,0.65]
            mapdata[i] =205;
          }
        }
      }
        
    memcpy(mapdata,&map->data[0],map->data.size());
    std::cout <<"recieve map buff size = "<<map->data.size() <<std::endl;
    //ready to press
    //MapSave::Instance()->MapSave_MapData("test_kkkk", (unsigned char *)&map->data[0] ,staticInstance->mappressed.uncompressed_length);
}
#define MYMAP "/home/nanorobot/Downloads/baqi.gif"//"/home/nvidia/0429.pgm"
int main(int argc, char **argv)
{
    string webServerPath = "/home/nvidia/tst/web_tst/crow-master/build/examples/templates/";
    ros::init(argc, argv, "laser_listener");
    ros::NodeHandle nh;
    ros::Subscriber sub = nh.subscribe("/scan", 10, laserCallback);
    ros::Subscriber sub_map = nh.subscribe("/map", 1000, mapCallback);
    
    crow::SimpleApp app;
    //crow::mustache::set_base("/home/nvidia/tst/web_tst/crow-master/build/examples/templates/");
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
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary = true){
                    std::lock_guard<std::mutex> _(mtx);
                    cout <<  "is_binary " << is_binary << endl;
                    for(auto u:users)
                    {
                        if (is_binary)
                        {
                            static int sendMapFlag = 1;
                            if(sendMapFlag)
                            {
                                for(int i =0; i < mapSize; i++)
                                {
                                    printf("0x%02x ",mapdata[i]);
                                    if((i+1) % 16 == 0)
                                     printf("\n");
                                }
       
                                string tmp((char*)mapdata,mapSize);
                                u->send_binary(tmp);
                                //sendMapFlag = 0£»
                            }
                            struct stat buf;

                            if( stat(MYMAP, &buf ) != -1 ) {
                                cout << "File size = " << buf.st_size << endl;
                            }

                            fstream f(MYMAP, ios::in | ios::binary);
                            if (f.bad())
                                cout << "bad file open" << endl;

                            unsigned char *img = new unsigned char[buf.st_size];
                            f.read((char*)img, buf.st_size);
                            f.close();
                            string imgstr((char *)img,buf.st_size);
                            //u->send_binary(imgstr);
                            //unsigned char aa[]= {0xcd,0xcd,0xff,0xff};
                            /*for(int i = 0; i < 1440; i++)
                            {
                                printf("%x ",laserscan_data[i]);
                                if(i % 32 == 0)
                                    printf("\n");
                            }
                            printf("\n");*/
                            //string tmp((char*)laserscan_data,360 * 4);
                            //u->send_binary(tmp);

                           /*
                            ofstream origin1("bb.dat");
                            origin1.write((char*)imgstr.data(),imgstr.size());
                            origin1.close();
                            cout << "imgstr.size " << imgstr.size() << endl;
                            *///cout << "binary  data " << data << endl;
                            for(int i = 0; i < data.size();i++)
                                printf("0x%x ",data[i]);
                            printf("\n");
                        }
                        else
                        {
                            u->send_text("qqqq");
                            //cout << "text data " << data << endl;
                        }
                    }
                });

    CROW_ROUTE(app, "/")
    ([]{
        char name[256];
        gethostname(name, 256);
        cout << "server name " << name << endl;
        crow::mustache::context x;
        x["servername"] = name;

        auto page = crow::mustache::load("ws.html");
        return page.render(x);
     });
/*
    app.route_dynamic("/<path>")
    ([&webServerPath](const crow::request& req, crow::response& resp, std::string path) {

        responseStaticResource(req, resp, webServerPath, path);
    });

    */
    std::thread tmpThrd(rosThrd);
    tmpThrd.detach();
    app.port(40080)
        .multithreaded()
        .run();
    
}
