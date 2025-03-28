#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <string>
#include <time.h>

using namespace std;


int ReturnStrings(const char* path, const int num, char* buf, const int size)
{
    int cnt = 0;
    FILE* file = NULL;

    if ((file = fopen(path, "r")) == NULL)
        return -1;

    while (fgets(buf, size, file) != NULL && num > ++cnt);
    fclose(file);

    if (num != cnt)
    {
        for (cnt = 0; cnt < size; ++cnt)
            buf[cnt] = 0;
        return -2;
    }

    return 0;
}

int makeFile(int t){
	
	
	char GPSR_0[128];
	char NGPSR_0[128];
	char PGPSR_0[128];
	char DGPSR_0[128];
	
    
	
	double GPSR[t],NGPSR[t],PGPSR[t],DGPSR[t];
	std::string value = "NULL";	
	
	for(int i=1;i<=8;i++){
		for(int l=1;l<=t;l++){
			
			
			std::string Gstr = "/home/hry-user/Simulation/GPSR/data" +std::to_string(l)+ ".txt";
			std::string Istr = "/home/hry-user/Simulation/NGPSR/data" +std::to_string(l)+ ".txt";
			std::string Hstr = "/home/hry-user/Simulation/PGPSR/data" +std::to_string(l)+ ".txt";
			std::string Dstr = "/home/hry-user/Simulation/DGPSR/data" +std::to_string(l)+ ".txt";

			
			
			ReturnStrings( Gstr.c_str(), i, GPSR_0, 128 );
			ReturnStrings( Istr.c_str(), i, NGPSR_0, 128 );
			ReturnStrings( Hstr.c_str(), i, PGPSR_0, 128 );
			ReturnStrings( Dstr.c_str(), i, DGPSR_0, 128 );

			
		
			GPSR[l] = atof(GPSR_0);
			NGPSR[l] = atof(NGPSR_0);
			PGPSR[l] = atof(PGPSR_0);
			DGPSR[l] = atof(DGPSR_0);
		
		switch(i){		
		case 1:
		value="Throughput";
		break;
		case 2:
		value="PDR";
		break;
		case 3:
		value="Overhead";
		break;
		case 4:
		value="Delay";
		break;
		case 5:
		value="Loss";
		break;
		case 6:
		value="HopNum";	
		break;
		case 7:
		value="SimTime";
		break;
		case 8:
		value="Memory";
		break;
		default:
		value="NULL";			
		}
		fstream fs;
		fs.open("/home/hry-user/Simulation/"+value+".txt", ios::out);
		fs << value<<","<<"GPSR"<<","<<"NGPSR"<<","<<"PGPSR"<<","<<"DGPSR"<<endl;
		
		for(int r=1; r<=t; r++){
			fs <<std::to_string(r)<<"回目"<<","<<std::to_string(GPSR[r])<<","<<std::to_string(NGPSR[r])<<","<<std::to_string(PGPSR[r])<<","<<std::to_string(DGPSR[r])<<endl;
		}
		fs.close();	
			
		}
		
	
	}
	return 0;
}
int main(int argc, char *argv[]){

	int t = atoi(argv[1]);//螳滄ｨ灘屓謨ｰ
	makeFile(t);
	
	
	return 0;

}
