/*
 * Rest API Server for Arduino
 * 
 *   This implements an REST server on arduino
 * 
 * Filipe C - fcaldas@canal-plus.fr
 */

#include "vector.h"
#include "routing.h" 

class restServer{
private:
  EthernetServer *server;
  vector<__route> routes;

  /*  Searches for a route that matches an user request
      Params:
        routename : Requested route
        rtype : Requested method (GET / POST)
   */
  __route *getRoute(char routename[], REQUEST_TYPE rtype){
    for(short i = 0; i < routes.size(); i++){
      if(strcmp(routes[i].routename,routename) == 0 &&
         routes[i].rtype == rtype)
         return &(routes[i]);
    }
    return NULL;
  }

  /*
   * Send HTTP Json app header
   */
  void sendHeader(EthernetClient *client){
    client->println(F("HTTP/1.1 200 OK"));
    client->println(F("Content-Type: application/json"));
    client->println(F("Access-Control-Allow-Origin: *"));
    client->println(F("Connection: Keep-Alive"));
    client->println(F(""));  
  }

  /*
   * Send a 404 error message to client
   */
  void send404(EthernetClient *client){
    sendHeader(client);
    client->println(F("{\"message\":\"Route not found\",\"error\":{\"status\":404}}"));
  }

  /*Receives a request and looks for a callback method to reply*/
  void reply(char routename[], REQUEST_TYPE rtype, 
             char json[], EthernetClient *client){
     __route* route = getRoute(routename, rtype);
     if(route){
        sendHeader(client);
        route->callback(client, json);
     }else{
        send404(client); 
     }
  }

  /*
   * Parses the user request and checks if its a GET or POST
   */
  __request * getRequest(EthernetClient &client){
      __request *req = new __request();
      bool readingFirst = true;
      char firstLine[MAX_REQUEST_SIZE];
      char page[ROUTESIZE];
      unsigned short pos = 0;     
      while (client.connected() && readingFirst) {
        if (client.available()) {
          char c = client.read();
          
          firstLine[pos] = c;
          pos++;
  
          //decode first line of the request
          if(c == '\r'){
            unsigned short start;
            firstLine[pos - 1] = '\0';
            char reqtype[5];
            strncpy(reqtype, firstLine,4);
            reqtype[4] == 0;
            if(strcmp(reqtype, "GET ") == 0){
              req->rtype = GET;
              start = 4;
            }else if(strcmp(reqtype, "POST") == 0){
              req->rtype = POST;
              start = 5;
            }
            pos = 0;
            
            for(; start < strlen(firstLine) && readingFirst; start++){
              page[pos] = firstLine[start];
              if(firstLine[start] == ' ' || firstLine[start] == '\0'){
                readingFirst = false;
              }else{
                pos++;
              }
            }
            page[pos] = 0x00;
          }          
        }
     }
     strcpy(req->routename, page);
     return req;
  }

  /*
   * Return the content of a POST message
   */
  char * getPostContent(EthernetClient &client){
     char * data = (char *) malloc(sizeof(char) * MAX_REQUEST_SIZE);
     unsigned short sizeData = 0;
     unsigned short nBraces = 0;
     bool finished = false;
     while (client.connected() && !finished ) {
        if (client.available()) {
          char c = client.read();
          data[sizeData] = c;
          sizeData++;
          if(c == '\n'){
            if(data[sizeData - 2] == '\r' && sizeData == 2){
              finished = true;
              sizeData = 0;
            }else{
              sizeData = 0;
            }
          }
          if(sizeData == MAX_REQUEST_SIZE){
            sizeData = 0;
          }
        }
     }
     finished = false;
     sizeData = 0;
     while (client.connected() && !finished ) {
        if (client.available()) {
          char c = client.read();
          if(c != '\r' && c != '\n' && c != ' '){
            data[sizeData] = c;
            sizeData++;
            if(c == '{'){
              nBraces++;
            }else if( c == '}'){
              nBraces--;
              if(nBraces == 0)
                finished = true;
            }
          }
        }
     }
     data[sizeData] = 0x00;
     return data;  
  }
  
public:
  //rest Server constructor
  restServer(byte mac[6], byte ip[4], byte gway[4], byte subnet[4], int port){
    Ethernet.begin(mac, ip);
    server = new EthernetServer(port);
    server->begin();
  }  

  /*
   * Adds a route to the route table
   */
  void addRoute(char name[], REQUEST_TYPE req, void (* callback)(EthernetClient *client, char args[])){
    routes.push_back(__route(name, req, callback));
  }

  /*
   * Function that waits for clients to connect
   * to be called on the program main loop on arduino.
   */
  void serve() {
    EthernetClient client = server->available();
    char *json;
    
    if (client) {
      //get header of request
      __request * req = getRequest(client);
      
      if(req->rtype == POST){
        json = getPostContent(client);
        reply(req->routename, req->rtype, json, &client);
        free(json);
      }else{
        reply(req->routename, req->rtype, json, &client);
      }
      free(req);
      client.flush();
      client.stop();
      Ethernet.maintain();
      
    }
  }
  
};
