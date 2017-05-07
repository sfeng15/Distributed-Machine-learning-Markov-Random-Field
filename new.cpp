#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;

enum DIRECTION {LEFT, RIGHT, UP, DOWN, DATA};

typedef unsigned int TYPE;

// parameters, specific to dataset
const int BP_ITERATIONS = 2;
const int LABELS = 16;
const int LAMBDA = 20;
const int SMOOTHNESS_TRUNC = 2;
const int S=1;
int BD;

struct Msg
{
	TYPE msg[LABELS+1];
};

struct MsgQ
{
	std::queue<Msg> msgs;
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

// Application specific code
void InitDataCost(const std::string &left_file, const std::string &right_file, MRF2D &mrf);
TYPE DataCostStereo(const cv::Mat &left, const cv::Mat &right, int x, int y, int label);
TYPE SmoothnessCost(int i, int j);

// Loppy belief propagation specific
void BP(MRF2D &mrf);
int SendMsg(MRF2D &mrf, int x, int y, int direction);
TYPE MAP(MRF2D &mrf);

int main()
{

    InitDataCost("tsukuba-imL.png", "tsukuba-imR.png", mrf);

    for(int i=0; i < BP_ITERATIONS; i++) {
        BP(mrf);
//        BP(mrf, LEFT);
//        BP(mrf, UP);
//        BP(mrf, DOWN);

        TYPE energy = MAP(mrf);

        cout << "iteration " << (i+1) << "/" << BP_ITERATIONS << ", energy = " << energy << endl;
    }

    cv::Mat output = cv::Mat::zeros(mrf.height, mrf.width, CV_8U);

    for(int y=LABELS; y < mrf.height-LABELS; y++) {
        for(int x=LABELS; x < mrf.width-LABELS; x++) {
            // Increase the intensity so we can see it
            output.at<uchar>(y,x) = mrf.grid[y*mrf.width+x].best_assignment * (256/LABELS);
        }
    }

//    cv::namedWindow("main", CV_WINDOW_AUTOSIZE);
//    cv::imshow("main", output);
//    cv::waitKey(0);

    cout << "Saving results to output.png" << endl;
    cv::imwrite("output.png", output);

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
    mrf.height = left.rows;

    int total = mrf.width*mrf.height;
	BD = total/2;

    mrf.grid.resize(total);
	sb.resize(total);
	rb.resize(total);

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
                mrf.grid[y*left.cols+x].msg[DATA][i] = DataCostStereo(left, right, x, y, i);
            }
        }
    }
}

int SendMsg(MRF2D &mrf, int x, int y, int direction)
{
    TYPE new_msg[LABELS+1];

    int width = mrf.width;
	int pos = y*width+x;


    for(int i=0; i < LABELS; i++) {
        TYPE min_val = UINT_MAX;

		if(!((pos-1)<BD)) {
int block=1;
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
if(block) return 0;
}
		if(!((pos+1)<BD)) {
int block=1;
while(!(rb[pos].mqs[RIGHT].msgs.empty())){
Msg tmp = rb[pos].mqs[RIGHT].msgs.front();
if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
for(int l=0; l<LABELS; l++){
mrf.grid[y*width+x].msg[RIGHT][l]=tmp.msg[l];
}
rb[pos].mqs[RIGHT].msgs.pop();
break;
}
else{
rb[pos].mqs[RIGHT].msgs.pop();
}
}
if(block) return 0;
}
		if(!((pos-width)<BD)) {
int block=1;
while(!(rb[pos].mqs[UP].msgs.empty())){
Msg tmp = rb[pos].mqs[UP].msgs.front();
if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
for(int l=0; l<LABELS; l++){
mrf.grid[y*width+x].msg[UP][l]=tmp.msg[l];
}
rb[pos].mqs[UP].msgs.pop();
break;
}
else{
rb[pos].mqs[UP].msgs.pop();
}
}
if(block) return 0;
}

		if(!((pos+width)<BD)) {
int block=1;
while(!(rb[pos].mqs[DOWN].msgs.empty())){
Msg tmp = rb[pos].mqs[DOWN].msgs.front();
if(abs(tmp.msg[LABELS]-mrf.grid[pos].itix)<=S){
for(int l=0; l<LABELS; l++){
mrf.grid[y*width+x].msg[DOWN][l]=tmp.msg[l];
}
rb[pos].mqs[DOWN].msgs.pop();
break;
}
else{
rb[pos].mqs[DOWN].msgs.pop();
}
}
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
			if ((y*width + x-1) < BD){
    for(int i=0; i < LABELS+1; i++) {
	            mrf.grid[y*width + x-1].msg[RIGHT][i] = new_msg[i];
    }
			}
			else{
				Msg tmp;
    for(int i=0; i < LABELS+1; i++) {
	            tmp.msg[i] = new_msg[i];
    }

				sb[pos].mqs[LEFT].msgs.push(tmp);
			}
            break;

            case 1:
			if ((y*width + x+1) < BD){
    for(int i=0; i < LABELS+1; i++) {
            mrf.grid[y*width + x+1].msg[LEFT][i] = new_msg[i];
    }
			}
			else{
				Msg tmp;
    for(int i=0; i < LABELS+1; i++) {
	            tmp.msg[i] = new_msg[i];
    }

				sb[pos].mqs[RIGHT].msgs.push(tmp);
			}
            break;

            case 2:
			if (((y-1)*width + x) < BD){
    for(int i=0; i < LABELS+1; i++) {
            mrf.grid[(y-1)*width + x].msg[DOWN][i] = new_msg[i];
    }
			}
			else{
				Msg tmp;
    for(int i=0; i < LABELS+1; i++) {
	            tmp.msg[i] = new_msg[i];
    }

				sb[pos].mqs[UP].msgs.push(tmp);
			}
            break;

            case 3:
			if (((y+1)*width + x) < BD){
    for(int i=0; i < LABELS+1; i++) {
            mrf.grid[(y+1)*width + x].msg[UP][i] = new_msg[i];
    }
			}
			else{
				Msg tmp;
    for(int i=0; i < LABELS+1; i++) {
	            tmp.msg[i] = new_msg[i];
    }

				sb[pos].mqs[DOWN].msgs.push(tmp);
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
if(!(((x==0)&&d==0)||((x==(width-1)&&d==1)) || (y==0&&d==2) || (y==(height-1)&&d==3))){
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
