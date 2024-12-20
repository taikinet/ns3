#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <string>
#include <time.h>
#include <vector>

using namespace std;


int ReturnStrings(const char* path, const int num, char* buf, const int size)
{
    int cnt = 0;
    FILE* file = NULL;

    // ファイルが存在しない、開けないならばエラー
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
    double avg_GPSR;
    double avg_NGPSR;
    double avg_PGPSR;
    double avg_DGPSR;
    double sum_GPSR = 0.0;
    double sum_NGPSR = 0.0;
    double sum_PGPSR = 0.0;
    double sum_DGPSR = 0.0;
    double max_GPSR = 0.0;  
    double max_NGPSR = 0.0;
    double max_PGPSR = 0.0;
    double max_DGPSR = 0.0;
    double min_GPSR = 1000000000; 
    double min_NGPSR = 10000000000;
    double min_PGPSR = 10000000000;
    double min_DGPSR = 10000000000;
	
	std::vector<std::string> traceFiles = {"mobility112.tcl"};    // ここいじる
    for (const std::string& traceFile : traceFiles) {


        double GPSR[t],NGPSR[t],PGPSR[t],DGPSR[t];
        std::string value = "NULL";	

        fstream fs;
        fs.open("/home/hry-user/Simulation/"+traceFile+"/avarage.txt", ios::out | ios::app);
        fs << traceFile << endl;
        fs << std::to_string(t) << "回の平均" << ", " << "GPSR" << ", " << "NGPSR" << ", " << "NPGPSR" << ", " << "NDGPSR" << endl;
        fs.close();
        
        for(int i=1;i<=10;i++){

            sum_GPSR = 0;
            sum_NGPSR = 0;
            sum_PGPSR = 0;
            sum_DGPSR = 0;
            max_GPSR = 0.0;  // 最初の値で初期化
            max_NGPSR = 0.0;
            max_PGPSR = 0.0;
            max_DGPSR = 0.0;
            min_GPSR = 1000000000;  // 最初の値で初期化
            min_NGPSR = 10000000000;
            min_PGPSR = 10000000000;
            min_DGPSR = 10000000000;


            if (i < 9){
                for(int l=1;l<=t;l++){
                
                
                    std::string Gstr = "/home/hry-user/Simulation/"+traceFile+"/GPSR/data" +std::to_string(l)+ ".txt";
                    std::string Istr = "/home/hry-user/Simulation/"+traceFile+"/NGPSR/data" +std::to_string(l)+ ".txt";
                    std::string Hstr = "/home/hry-user/Simulation/"+traceFile+"/NPGPSR/data" +std::to_string(l)+ ".txt";
                    std::string Dstr = "/home/hry-user/Simulation/"+traceFile+"/NDGPSR/data" +std::to_string(l)+ ".txt";

                    
                    
                    ReturnStrings( Gstr.c_str(), i, GPSR_0, 128 );
                    ReturnStrings( Istr.c_str(), i, NGPSR_0, 128 );
                    ReturnStrings( Hstr.c_str(), i, PGPSR_0, 128 );
                    ReturnStrings( Dstr.c_str(), i, DGPSR_0, 128 );

                    
                
                    GPSR[l] = atof(GPSR_0);
                    NGPSR[l] = atof(NGPSR_0);
                    PGPSR[l] = atof(PGPSR_0);
                    DGPSR[l] = atof(DGPSR_0);
                }

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
                fs.open("/home/hry-user/Simulation/"+traceFile+"/"+value+".txt", ios::out);
                fs << value<<","<<"GPSR"<<","<<"NGPSR"<<","<<"NPGPSR"<<","<<"NDGPSR"<<endl;
                
                for(int r=1; r<=t; r++){
                    fs <<std::to_string(r)<<"回目"<<","<<std::to_string(GPSR[r])<<","<<std::to_string(NGPSR[r])<<","<<std::to_string(PGPSR[r])<<","<<std::to_string(DGPSR[r])<<endl;
                    sum_GPSR += GPSR[r];
                    sum_NGPSR += NGPSR[r];
                    sum_PGPSR += PGPSR[r];
                    sum_DGPSR += DGPSR[r];
                    if (GPSR[r] > max_GPSR) max_GPSR = GPSR[r];
                    if (NGPSR[r] > max_NGPSR) max_NGPSR = NGPSR[r];
                    if (PGPSR[r] > max_PGPSR) max_PGPSR = PGPSR[r];
                    if (DGPSR[r] > max_DGPSR) max_DGPSR = DGPSR[r];
                    if (GPSR[r] < min_GPSR) min_GPSR = GPSR[r];
                    if (NGPSR[r] < min_NGPSR) min_NGPSR = NGPSR[r];
                    if (PGPSR[r] < min_PGPSR) min_PGPSR = PGPSR[r];
                    if (DGPSR[r] < min_DGPSR) min_DGPSR = DGPSR[r];
                }
                fs.close();	

                avg_GPSR = sum_GPSR / t;
                avg_NGPSR = sum_NGPSR / t;
                avg_PGPSR = sum_PGPSR / t;
                avg_DGPSR = sum_DGPSR / t;

                fs.open("/home/hry-user/Simulation/"+traceFile+"/avarage.txt", ios::out | ios::app);
                fs << value << ", " << avg_GPSR << ", " << avg_NGPSR << ", " << avg_PGPSR << ", " << avg_DGPSR << endl;
                fs << "最大値" << ", " << max_GPSR << ", " << max_NGPSR << ", " << max_PGPSR << ", " << max_DGPSR << endl;
                fs << "最小値" << ", " << min_GPSR << ", " << min_NGPSR << ", " << min_PGPSR << ", " << min_DGPSR << endl;
                fs << "--------------------------------" << endl;
                fs.close();

            } else {
                for(int l=1;l<=t;l++){
                
                    std::string Hstr = "/home/hry-user/Simulation/"+traceFile+"/NPGPSR/data" +std::to_string(l)+ ".txt";
                    std::string Dstr = "/home/hry-user/Simulation/"+traceFile+"/NDGPSR/data" +std::to_string(l)+ ".txt";

                    ReturnStrings( Hstr.c_str(), i, PGPSR_0, 128 );
                    ReturnStrings( Dstr.c_str(), i, DGPSR_0, 128 );

                    PGPSR[l] = atof(PGPSR_0);
                    DGPSR[l] = atof(DGPSR_0);
                }

                switch(i){		
                    case 9:
                    value="GenerateSignatureTime";
                    break;
                    case 10:
                    value="VerifySignatureTime";
                    break;
                    default:
                    value="NULL";			
                }

                fstream fs;
                fs.open("/home/hry-user/Simulation/"+traceFile+"/"+value+".txt", ios::out);
                fs << value<<","<<"NPGPSR"<<","<<"NDGPSR"<<endl;
                
                for(int r=1; r<=t; r++){
                    fs <<std::to_string(r)<<"回目"<<","<<std::to_string(PGPSR[r])<<","<<std::to_string(DGPSR[r])<<endl;
                    sum_PGPSR += PGPSR[r];
                    sum_DGPSR += DGPSR[r];
                    if (PGPSR[r] > max_PGPSR) max_PGPSR = PGPSR[r];
                    if (DGPSR[r] > max_DGPSR) max_DGPSR = DGPSR[r];
                    if (PGPSR[r] < min_PGPSR) min_PGPSR = PGPSR[r];
                    if (DGPSR[r] < min_DGPSR) min_DGPSR = DGPSR[r];
                }
                fs.close();	

                avg_PGPSR = sum_PGPSR / t;
                avg_DGPSR = sum_DGPSR / t;

                fs.open("/home/hry-user/Simulation/"+traceFile+"/avarage.txt", ios::out | ios::app);
                fs << value << ", " << "署名なし" << ", " << "署名計算なし" << ", " << avg_PGPSR << ", " << avg_DGPSR << endl;
                fs << "最大値" << ", " << "署名なし" << ", " << "署名計算なし" << ", " << max_PGPSR << ", " << max_DGPSR << endl;
                fs << "最小値" << ", " << "署名なし" << ", " << "署名計算なし" << ", " << min_PGPSR << ", " << min_DGPSR << endl;
                fs << "--------------------------------" << endl;
                fs.close();
                    
            }
        }
    }
	return 0;
}


int main(int argc, char *argv[]){

	int t = atoi(argv[1]);
	makeFile(t);
	
	
	return 0;

}
