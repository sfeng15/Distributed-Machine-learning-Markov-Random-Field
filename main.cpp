#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cmath>
#include <ctime>       /* time_t, struct tm, difftime, time, mktime */


/*
** server.c -- a stream socket server demo
*/

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <typeinfo>	

#include <sys/wait.h>
#include <signal.h>

using namespace std;

enum DIRECTION {LEFT, RIGHT, UP, DOWN, DATA};

#define num 2

//#define D

typedef unsigned int TYPE;

// parameters, specific to dataset
const int BP_ITERATIONS = 2;
const int LABELS = 16;
const int LAMBDA = 20;
const int SMOOTHNESS_TRUNC = 2;
const int S=1;
int BD;
int mid=1;

std::mutex* mts;

struct Msg
{
    TYPE msg[LABELS+1];
};

struct MsgQ
{
    std::queue<Msg> msgs;
    //	std::mutex mt;
};

struct RecvPix{
    MsgQ mqs[4];
};

std::vector <RecvPix> rb; 

struct SendPix{
    MsgQ mqs[4];
};

std::vector <SendPix> sb;

struct Pixel
{
    // Each pixel has 5 'message box' to store incoming data
    TYPE msg[5][LABELS+1];
    int best_assignment;
    int itix;
    int direction;
};

struct MRF2D
{
    std::vector <Pixel> grid;
    int width, height;
};

MRF2D mrf;

int iter;

int server_start_pixel=(mrf.height-1)*mrf.width ;// the port users will be connecting to
int server_end_pixel= (mrf.height-1)*mrf.width+mrf.width; // the port users will be connecting to


int client_start_pixel=(mrf.height)*mrf.width ;// the port users will be connecting to
int client_end_pixel= (mrf.height)*mrf.width+mrf.width; // the port users will be connecting to


#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hold
void sigchld_handler(int s)
{
 // waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


#define CHANGE_RATE 0.01
#define QUEUE_END "9999"
#define MAXDATASIZE 16 // max number of bytes we can get at once
#define MAXPIXEL 200





void ClientSender(int sockfd) {

	std::cout << "client Send thread start" << std::endl;

	cout<<"start pixel"<<client_start_pixel<<endl;
	cout<<"end pixel"<<client_end_pixel<<endl;

	while(true){
		for (int i = client_start_pixel; i < client_end_pixel; ++i)
		{
			// std::cout << "Send thread iterate over pixel: " << i << std::endl;
			SendPix pix=sb[i];

			
			//client is the bottom image, use the 3rd up message queue
			std::queue<Msg> mq=pix.mqs[2].msgs;
			
			// cout<<__LINE__<<endl;

			while (!mq.empty())
			{

				// cout<<__LINE__<<endl;
				Msg msg1=mq.front();
				int integer;
				for (int i = 0; i < 16; ++i)
				{
					// cout<<msg1.msg[i]<<endl;
					integer=int (msg1.msg[i]);
			
					string S = std::to_string(integer) + ",";
					const char *cstr = S.c_str();
					
					std::cout << "Send thread sends: " << S << std::endl;
					std::cout << sizeof(cstr) << std::endl;
					if(send(sockfd, cstr, strlen(cstr), 0)==-1)
						perror("send");
				}
				mts[i*8+2].lock();
				mq.pop();
				mts[i*8+2].unlock();
			}
			if(send(sockfd, QUEUE_END, strlen(QUEUE_END), 0)==-1)
				perror("send");
		}

	}
	std::cout << "client Send thread exit" << std::endl;


}

int ClientReceiver(int argc, char *argv[]){

	std::cout << "client receive thread start" << std::endl;







	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	if (argc != 4) {
		fprintf(stderr,"wrong input format: \n");
		exit(1);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	cout<<"server IP is: "<<argv[2]<<endl;

	if ((rv = getaddrinfo(argv[2], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
 	}
 	// loop through all the results and connect to the first we can
 	for(p = servinfo; p != NULL; p = p->ai_next) {
	 	if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
		 	continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		 	close(sockfd);
		 	perror("client: connect");
		 	continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
	s, sizeof s);
	printf("client: connecting to %s\n", s);
	freeaddrinfo(servinfo); // all done with this structure



	int  numbytes;
	int idxPixel=0;
	int label=0;
	string partial;


	//start client sender thread
	//
	//
	std::thread t2(ClientSender,sockfd);

	//loop to receive messages
	//
	//
	//
 	while(true){
	  	char buf[MAXDATASIZE];


	 	if ((numbytes = recv(sockfd, buf, sizeof(buf), 0)) == -1) {
			perror("recv");
			exit(1);
		}
		buf[numbytes] = '\0';
		string S(buf);
		
		// cout << "Receive thread receives: " << S << endl;


		RecvPix pix=rb[idxPixel];
		//msgQ[2] is the up queue, client uses
		std::queue<Msg> mq=pix.mqs[2].msgs;
		Msg msg1;

		for (unsigned int i = 0; i < S.length(); i++) {
			if (S[i]==',')
			{
				int i_dec = std::stoi (partial);

				// cout << "Receive thread receives: " << i_dec << endl;


				if (i_dec==std::stoi(QUEUE_END))
				{
					idxPixel=(idxPixel+1)%MAXPIXEL;
				
					RecvPix pix=rb[idxPixel];

					//msgQ[2] is the up queue
					std::queue<Msg> mq=pix.mqs[2].msgs;
					label=0;

				}


				msg1.msg[label++]=TYPE(i_dec);
				if (label==16)
				{
					label=0;
					mts[idxPixel*8+4+2].lock();
					// cout<<__LINE__<<endl;
					mq.push(msg1);
					mts[idxPixel*8+4+2].unlock();
				}

				partial = string();
			} else {
				partial += S[i];
			}
		}
 	}
 	std::cout << "client receive thread exit" << std::endl;

	t2.join();
	close(sockfd);

}



void ServerReceiver(int new_fd) {
	std::cout << "server receive thread start" << std::endl;

  	char buf[MAXDATASIZE];

 	int  numbytes;
 	string partial;
 	int idxPixel=0;
 	int label=0;


 	while(true){

	 	if ((numbytes = recv(new_fd, buf, sizeof(buf), 0)) == -1) {
			perror("recv");
			exit(1);
		}

		buf[numbytes] = '\0';
		string S(buf);
		// cout << "Receive thread receives: " << S << endl;

		RecvPix pix=rb[idxPixel];

		//msgQ[3] is the down queue
		std::queue<Msg> mq=pix.mqs[3].msgs;
		Msg msg1;


		for (unsigned int i = 0; i < S.length(); i++) {
			if (S[i]==',')
			{
				int i_dec = std::stoi (partial);
				// cout << typeid(i_dec).name() << endl;
				// cout << "Receive thread receives: " << i_dec << endl;



				if (i_dec==std::stoi(QUEUE_END))
				{
					idxPixel=(idxPixel+1)%MAXPIXEL;
				
					RecvPix pix=rb[idxPixel];

					//msgQ[3] is the down queue
					std::queue<Msg> mq=pix.mqs[3].msgs;
					label=0;
				}
			

				msg1.msg[label++]=TYPE(i_dec);
				if (label==16)
				{
					label=0;
					mts[idxPixel*8+4+3].lock();
					// cout<<__LINE__<<endl;
					mq.push(msg1);
					mts[idxPixel*8+4+3].unlock();
				}
	

				partial = string();
			} else {
				partial += S[i];
			}
		}
 	}
 	std::cout << "server receive thread exit" << std::endl;

  	
}
int setUpServerSocket(int sockfd){
	int new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
			sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo); // all done with this structure
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	printf("server: waiting for connections...\n");
	

	sin_size = sizeof their_addr;
	new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
	if (new_fd == -1) {
		perror("accept");
		// continue;
	}
	inet_ntop(their_addr.ss_family,
	get_in_addr((struct sockaddr *)&their_addr),
	s, sizeof s);
	printf("server: got connection from %s\n", s);

	return new_fd;

}
int ServerSender(){
	std::cout << "server Send thread start" << std::endl;

	//setup socket connection
	int sockfd=0;
	int new_fd=setUpServerSocket(sockfd);

	//throw receive thread
	std::thread t2(ServerReceiver,new_fd);
	
	//send messages
	//
	//
	//
	cout<<"start pixel"<<server_start_pixel<<endl;
	cout<<"end pixel"<<server_end_pixel<<endl;

	for (int i = server_start_pixel; i < server_end_pixel; ++i)
	{
		// std::cout << "Send thread iterate over pixel: " << i << std::endl;

		SendPix pix=sb[i];

		//server uses down queue(4th)
		std::queue<Msg> mq=pix.mqs[3].msgs;

		while (!mq.empty())
		{
			Msg msg1=mq.front();
			int integer;
			for (int i = 0; i < 16; ++i)
			{
				// cout<<msg1.msg[i]<<endl;
				integer=int (msg1.msg[i]);
		
				string S = std::to_string(integer) + ",";
				const char *cstr = S.c_str();
				
				std::cout << "Send thread sends: " << S << std::endl;
				std::cout << sizeof(cstr) << std::endl;
				if(send(new_fd, cstr, strlen(cstr), 0)==-1)
					perror("send");
			}
			mts[i*8+3].lock();
			mq.pop();
			mts[i*8+3].unlock();
		}
		if(send(sockfd, QUEUE_END, strlen(QUEUE_END), 0)==-1)
			perror("send");
	}
	t2.join();
	std::cout << "server receive thread exit" << std::endl;

	close(new_fd); 
	return 1;
}




// Application specific code
void InitDataCost(const std::string &left_file, const std::string &right_file, MRF2D &mrf);
TYPE DataCostStereo(const cv::Mat &left, const cv::Mat &right, int x, int y, int label);
TYPE SmoothnessCost(int i, int j);

// Loppy belief propagation specific
void BP(MRF2D &mrf);
int SendMsg(MRF2D &mrf, int x, int y, int direction);
TYPE MAP(MRF2D &mrf);

int main(int argc, char *argv[])
{
	if(strcmp(argv[3],"1")==0)
		mid=1;
	else
		mid=0;

	cout<<"machine number is: "<<mid<<endl;
	InitDataCost("tsukuba-imL.png", "tsukuba-imR.png", mrf);



   time_t now = time(0);
   cout << "start time: " << now << endl;


	TYPE preEnergy=0;
	TYPE curEnergy=0;
	if (strcmp(argv[1],"server")==0)
	{

		std::thread t1(ServerSender);

		int i=0;
		do{
			  BP(mrf);
	        //        BP(mrf, LEFT);
	        //        BP(mrf, UP);
	        //        BP(mrf, DOWN);
			 preEnergy=curEnergy;
	        curEnergy = MAP(mrf);


	        cout << "iteration " << (i+1) << "/" << BP_ITERATIONS << ", energy = " << curEnergy << endl;
	        i++;
		}while((double)(curEnergy-preEnergy)/(double)preEnergy>=CHANGE_RATE);

		 now = time(0);
		 cout << "end time: " << now << endl;
 

	    // for(int i=0; i < BP_ITERATIONS; i++) {
	    //     BP(mrf);
	    //     //        BP(mrf, LEFT);
	    //     //        BP(mrf, UP);
	    //     //        BP(mrf, DOWN);

	    //     TYPE energy = MAP(mrf);

	    //     cout << "iteration " << (i+1) << "/" << BP_ITERATIONS << ", energy = " << energy << endl;
	    // }

	    cv::Mat output = cv::Mat::zeros(mrf.height, mrf.width, CV_8U);

	    for(int y=LABELS; y < mrf.height-LABELS; y++) {
	        for(int x=LABELS; x < mrf.width-LABELS; x++) {
	            // Increase the intensity so we can see it
	            output.at<uchar>(y,x) = mrf.grid[y*mrf.width+x].best_assignment * (256/LABELS);
	            //			cout << mrf.grid[y*mrf.width+x].itix <<" ";
	        }
	    }

	    //    cv::namedWindow("main", CV_WINDOW_AUTOSIZE);
	    //    cv::imshow("main", output);
	    //    cv::waitKey(0);

	    cout << "Saving results to output.png" << endl;
	    cv::imwrite("output.png", output);

	    delete[] mts;

	    t1.join();

	}
	else{
		std::thread t1(ClientReceiver,argc, argv);
		// for(int i=0; i < BP_ITERATIONS; i++) {
	 //        BP(mrf);
	 //        //        BP(mrf, LEFT);
	 //        //        BP(mrf, UP);
	 //        //        BP(mrf, DOWN);

	 //        TYPE energy = MAP(mrf);

	 //        cout << "iteration " << (i+1) << "/" << BP_ITERATIONS << ", energy = " << energy << endl;
	 //    }
		int i=0;
		do{
			  BP(mrf);
	        //        BP(mrf, LEFT);
	        //        BP(mrf, UP);
	        //        BP(mrf, DOWN);
			 preEnergy=curEnergy;
	        curEnergy = MAP(mrf);


	        cout << "iteration " << (i+1) << "/" << BP_ITERATIONS << ", energy = " << curEnergy << endl;
	        i++;
		}while((double)(curEnergy-preEnergy)/(double)preEnergy>=CHANGE_RATE);

		now = time(0);
		 cout << "end time: " << now << endl;

	    cv::Mat output = cv::Mat::zeros(mrf.height, mrf.width, CV_8U);

	    for(int y=LABELS; y < mrf.height-LABELS; y++) {
	        for(int x=LABELS; x < mrf.width-LABELS; x++) {
	            // Increase the intensity so we can see it
	            output.at<uchar>(y,x) = mrf.grid[y*mrf.width+x].best_assignment * (256/LABELS);
	            //			cout << mrf.grid[y*mrf.width+x].itix <<" ";
	        }
	    }


	    //    cv::namedWindow("main", CV_WINDOW_AUTOSIZE);
	    //    cv::imshow("main", output);
	    //    cv::waitKey(0);

	    cout << "Saving results to output.png" << endl;
	    cv::imwrite("output.png", output);

	    delete[] mts;

	    t1.join();
	}

	
    return 0;
}

TYPE DataCostStereo(const cv::Mat &left, const cv::Mat &right, int x, int y, int label)
{
    const int wradius = 2; // window radius, search block size is (wradius*2+1)*(wradius*2+1)

    int sum = 0;

    for(int dy=-wradius; dy <= wradius; dy++) {
        for(int dx=-wradius; dx <= wradius; dx++) {
            int a = left.at<uchar>(y+dy, x+dx);
            int b = right.at<uchar>(y+dy, x+dx-label);
            sum += abs(a-b);
        }
    }

    int avg = sum/((wradius*2+1)*(wradius*2+1)); // average difference

    return avg;
    //return std::min(avg, DATA_TRUNC);
}

TYPE SmoothnessCost(int i, int j)
{
    int d = i - j;

    return LAMBDA*std::min(abs(d), SMOOTHNESS_TRUNC);
}

void InitDataCost(const std::string &left_file, const std::string &right_file, MRF2D &mrf)
{
    // Cache the data cost results so we don't have to recompute it every time

    // Force greyscale
    cv::Mat left = cv::imread(left_file.c_str(), 0);
    cv::Mat right = cv::imread(right_file.c_str(), 0);

    if(!left.data) {
        cerr << "Error reading left image" << endl;
        exit(1);
    }

    if(!right.data) {
        cerr << "Error reading right image" << endl;
        exit(1);
    }

    assert(left.channels() == 1);

    mrf.width = left.cols;
    mrf.height = left.rows/num;

    int total = mrf.width*mrf.height;
    BD = total;

    mrf.grid.resize(total);

    sb.resize(total);
    rb.resize(total);

    



    struct Msg tempMsg;
    for (int i = 0; i < 16; ++i)
    {
    	tempMsg.msg[i]=0;
    }

    for (int i = 0; i < total; ++i)
    {
	    for (int j = 0; j < 4; ++j)
	    {
	    	    rb[i].mqs[j].msgs.push(tempMsg);
	    }
    }




    mts = new std::mutex[total*8];

    // Initialise all messages to zero
    for(int i=0; i < total; i++) {
        for(int j=0; j < 5; j++) {
            for(int k=0; k < LABELS+1; k++) {
                mrf.grid[i].msg[j][k] = 0;
            }
        }
    }

    // Add a border around the image
    int border = LABELS;

    for(int y=border; y < mrf.height-border; y++) {
        for(int x=border; x < mrf.width-border; x++) {
            for(int i=0; i < LABELS; i++) {
                mrf.grid[y*left.cols+x].msg[DATA][i] = DataCostStereo(left, right, x, y+mid*(mrf.height), i);
            }
        }
    }
    cout<<"size of send buffer: "<<sb.size()<<endl;

    client_start_pixel=0 ;// the port users will be connecting to
	client_end_pixel= mrf.width; // the port users will be connecting to
	server_start_pixel=(mrf.height-1)*mrf.width ;// the port users will be connecting to
	server_end_pixel= (mrf.height-1)*mrf.width+mrf.width; // the port users will be connecting to

}

int SendMsg(MRF2D &mrf, int x, int y, int direction)
{
    TYPE new_msg[LABELS+1];





    int width = mrf.width;
    int pos = y*width+x;
    int gpos = pos + BD*mid;


	// if (mid==1&&pos==1)
 //    {
 //    	cout<<"pos: "<<pos<<endl;
 //    }



    for(int i=0; i < LABELS; i++) {
        TYPE min_val = UINT_MAX;

        if(!((mid*BD<=(gpos-1))&&((gpos-1)<(mid+1)*BD))&&
                !((!(gpos%width))||(gpos%width==(width-1)) || (gpos<width) || (gpos >= num*BD-width))) {
#ifdef D 
            cout << "L" <<endl; 
#endif
            int block=1;
            mts[pos*8+4].lock();
            while(!(rb[pos].mqs[LEFT].msgs.empty())){
                Msg tmp = rb[pos].mqs[LEFT].msgs.front();
                if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
                    for(int l=0; l<LABELS; l++){
                        mrf.grid[y*width+x].msg[LEFT][l]=tmp.msg[l];
                    }
                    block=0;
                    rb[pos].mqs[LEFT].msgs.pop();
                    break;
                }
                else{
                    rb[pos].mqs[LEFT].msgs.pop();
                }
            }
            mts[pos*8+4].unlock();
            if(block) return 0;
        }
        if (!((mid*BD<=(gpos+1))&&((gpos+1)<(mid+1)*BD)) &&
                !((!(gpos%width))||(gpos%width==(width-1)) || (gpos<width) || (gpos >= num*BD-width))) {
#ifdef D    
            cout << "R" <<endl;
#endif
            int block=1;
            mts[pos*8+4+1].lock();
            while(!(rb[pos].mqs[RIGHT].msgs.empty())){
                Msg tmp = rb[pos].mqs[RIGHT].msgs.front();
                if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
                    for(int l=0; l<LABELS; l++){
                        mrf.grid[y*width+x].msg[RIGHT][l]=tmp.msg[l];
                    }
                    block=0;
                    rb[pos].mqs[RIGHT].msgs.pop();
                    break;
                }
                else{
                    rb[pos].mqs[RIGHT].msgs.pop();
                }
            }
            mts[pos*8+4+1].unlock();
            if(block) return 0;
        }
        if(!((mid*BD<=(gpos-width))&&((gpos-width)<(mid+1)*BD))&&
                !((!(gpos%width))||(gpos%width==(width-1)) || (gpos<width) || (gpos >= num*BD-width))) {
#ifdef D 
            cout << "U" <<endl; 
#endif
            int block=1;
            mts[pos*8+4+2].lock();
            while(!(rb[pos].mqs[UP].msgs.empty())){
                Msg tmp = rb[pos].mqs[UP].msgs.front();
                if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
                    for(int l=0; l<LABELS; l++){
                        mrf.grid[y*width+x].msg[UP][l]=tmp.msg[l];
                    }
                    block=0;
                    rb[pos].mqs[UP].msgs.pop();
                    break;
                }
                else{
                    rb[pos].mqs[UP].msgs.pop();
                }
            }
            mts[pos*8+4+2].unlock();
//            cout<<block<<endl;
            if(block) return 0;
        }

        if(!((mid*BD<=(gpos+width))&&((gpos+width)<(mid+1)*BD))&&
                !((!(gpos%width))||(gpos%width==(width-1)) || (gpos<width) || (gpos >= num*BD-width))) {
#ifdef D
            cout << "D" <<endl; 
#endif
            int block=1;
            mts[pos*8+4+3].lock();
            while(!(rb[pos].mqs[DOWN].msgs.empty())){
                Msg tmp = rb[pos].mqs[DOWN].msgs.front();
                if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
                    for(int l=0; l<LABELS; l++){
                        mrf.grid[y*width+x].msg[DOWN][l]=tmp.msg[l];
                    }
                    block=0;
                    rb[pos].mqs[DOWN].msgs.pop();
                    break;
                }
                else{
                    rb[pos].mqs[DOWN].msgs.pop();
                }
            }
            mts[pos*8+4+3].unlock();
            if(block) return 0;
        }

        for(int j=0; j < LABELS; j++) {
            TYPE p = 0;

            p += SmoothnessCost(i,j);
            p += mrf.grid[y*width+x].msg[DATA][j];

            // Exclude the incoming message direction that we are sending to
            if(direction != 0) p += mrf.grid[y*width+x].msg[LEFT][j];
            if(direction != 1) p += mrf.grid[y*width+x].msg[RIGHT][j];
            if(direction != 2) p += mrf.grid[y*width+x].msg[UP][j];
            if(direction != 3) p += mrf.grid[y*width+x].msg[DOWN][j];

            min_val = std::min(min_val, p);
        }

        new_msg[i] = min_val;
    }

    new_msg[LABELS] = mrf.grid[pos].itix;

    switch(direction) {
        case 0:
            if (((mid*BD<=(gpos-1))&&((gpos-1)<(mid+1)*BD))){
                for(int i=0; i < LABELS+1; i++) {
                    mrf.grid[y*width + x-1].msg[RIGHT][i] = new_msg[i];
                }
            }
            else{
                Msg tmp;
                for(int i=0; i < LABELS+1; i++) {
                    tmp.msg[i] = new_msg[i];
                }

                mts[pos*8+direction].lock();
                sb[pos].mqs[LEFT].msgs.push(tmp);
                mts[pos*8+direction].unlock();
            }
            break;

        case 1:
            if (((mid*BD<=(gpos+1))&&((gpos+1)<(mid+1)*BD))){
                for(int i=0; i < LABELS+1; i++) {
                    mrf.grid[y*width + x+1].msg[LEFT][i] = new_msg[i];
                }
            }
            else{
                Msg tmp;
                for(int i=0; i < LABELS+1; i++) {
                    tmp.msg[i] = new_msg[i];
                }

                mts[pos*8+direction].lock();
                sb[pos].mqs[RIGHT].msgs.push(tmp);
                mts[pos*8+direction].unlock();
            }
            break;

        case 2:
            if (((mid*BD<=(gpos-width))&&((gpos-width)<(mid+1)*BD))){
                
                for(int i=0; i < LABELS+1; i++) {
                    mrf.grid[(y-1)*width + x].msg[DOWN][i] = new_msg[i];
                }
            }
            else{

                Msg tmp;
                for(int i=0; i < LABELS+1; i++) {
                    tmp.msg[i] = new_msg[i];
                }

                mts[pos*8+direction].lock();
                cout<<__LINE__<<pos<<endl;
                sb[pos].mqs[UP].msgs.push(tmp);
                mts[pos*8+direction].unlock();
            }
            break;

        case 3:
            if (((mid*BD<=(gpos+width))&&((gpos+width)<(mid+1)*BD))){
                for(int i=0; i < LABELS+1; i++) {
                    mrf.grid[(y+1)*width + x].msg[UP][i] = new_msg[i];
                }
            }
            else{
                Msg tmp;
                for(int i=0; i < LABELS+1; i++) {
                    tmp.msg[i] = new_msg[i];
                }

                mts[pos*8+direction].lock();
                cout<<__LINE__<<" "<<pos<<endl;
                sb[pos].mqs[DOWN].msgs.push(tmp);
                mts[pos*8+direction].unlock();
            }
            break;

        default:
            assert(0);
            break;
    }
    return 1;
}

void BP(MRF2D &mrf)
{
    int width = mrf.width;
    int height = mrf.height;


    for(int y=0; y < height; y++) {
        for(int x=0; x < width; x++) {
            int pos = y*width+x;
            int it_flag = 1;
            for(int d=mrf.grid[pos].direction; d<4;d++){
                int succ=0;
                if(!(((x==0)&&d==0)||((x==(width-1)&&d==1)) || (y+mid*height==0&&d==2) || (y+mid*height==(height*num-1)&&d==3))){
                    //
                    succ=SendMsg(mrf, x, y, d);
                    if(!succ){
                        mrf.grid[pos].direction = d;
                        break;
                    }
                }
                else{
                    succ = 1;
                }
                it_flag*=succ;
            }
            if (it_flag) {
                (mrf.grid[pos].itix)++;
                mrf.grid[pos].direction = 0;
            }
        }
    }
    /*    switch(direction) {
          case RIGHT:
          for(int y=0; y < height; y++) {
          for(int x=0; x < width-1; x++) {
          SendMsg(mrf, x, y, direction);
          }
          }
          break;

          case LEFT:
          for(int y=0; y < height; y++) {
          for(int x=width-1; x >= 1; x--) {
          SendMsg(mrf, x, y, direction);
          }
          }
          break;

          case DOWN:
          for(int x=0; x < width; x++) {
          for(int y=0; y < height-1; y++) {
          SendMsg(mrf, x, y, direction);
          }
          }
          break;

          case UP:
          for(int x=0; x < width; x++) {
          for(int y=height-1; y >= 1; y--) {
          SendMsg(mrf, x, y, direction);
          }
          }
          break;

          case DATA:
          assert(0);
          break;
          }*/
}

TYPE MAP(MRF2D &mrf)
{
    // Finds the MAP assignment as well as calculating the energy

    // MAP assignment
    for(size_t i=0; i < mrf.grid.size(); i++) {
        TYPE best = std::numeric_limits<TYPE>::max();
        for(int j=0; j < LABELS; j++) {
            TYPE cost = 0;

            cost += mrf.grid[i].msg[LEFT][j];
            cost += mrf.grid[i].msg[RIGHT][j];
            cost += mrf.grid[i].msg[UP][j];
            cost += mrf.grid[i].msg[DOWN][j];
            cost += mrf.grid[i].msg[DATA][j];

            if(cost < best) {
                best = cost;
                mrf.grid[i].best_assignment = j;
            }
        }
    }

    int width = mrf.width;
    int height = mrf.height;

    // Energy
    TYPE energy = 0;

    for(int y=0; y < mrf.height; y++) {
        for(int x=0; x < mrf.width; x++) {
            int cur_label = mrf.grid[y*width+x].best_assignment;

            // Data cost
            energy += mrf.grid[y*width+x].msg[DATA][cur_label];

            if(x-1 >= 0)     energy += SmoothnessCost(cur_label, mrf.grid[y*width+x-1].best_assignment);
            if(x+1 < width)  energy += SmoothnessCost(cur_label, mrf.grid[y*width+x+1].best_assignment);
            if(y-1 >= 0)     energy += SmoothnessCost(cur_label, mrf.grid[(y-1)*width+x].best_assignment);
            if(y+1 < height) energy += SmoothnessCost(cur_label, mrf.grid[(y+1)*width+x].best_assignment);
        }
    }

    return energy;
}
