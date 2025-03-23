LINE_NOTIFY_API="https://notify-api.line.me/api/notify"
TOKEN="uSmbrMW1BuCGlvovL2GlDPXbIoYqvOuMLVFRPcoUkUn"


rm -rf ~/dataTemp
rm -rf ~/Simulation/* #dataファイルの下を削除する


# ラインに送信
send_line_notification1() { 
	message="シミュレーション開始！  ↓--------------------------↓"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}
send_line_notification2() { 
	protocol=$1
	message="$protocol シミュレーション $i 回目終了"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}
send_line_notification3() { 
	traceFile=$1
    nodeCount=$2
    simulationTime=$3
	message="$traceFile,  ノード数：${nodeCount},  simTime : ${simulationTime}   終了"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

send_line_notification4() {
    HH=$HH
    MM=$MM
    SS=$SS
	message="シミュレーションが終了しました。  シュミレーション時間${HH}:${MM}:${SS}"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

send_line_notification5() {
    traceFile=$1
    file_path="/home/hry-user/Simulation/$traceFile/avarage.txt"
    if [ -f "$file_path" ]; then
        message=$(<"$file_path")
    fi
    curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}


start_time=`date +%s` 

i=1 #loop
r=5 #実験回数   # ここいじる
nodeCount=0
simlationTime=0
send_line_notification1
for traceFile in mobility112.tcl # mobility37_185.tcl mobility74_185.tcl mobility112_185.tcl mobility150_185.tcl mobility185.tcl   # mobility37.tcl mobility112.tcl mobility185.tcl  ### ここいじる
do
	if [ $traceFile == "mobility112.tcl" ]; then
        fileName="mobility112.tcl"
		nodeCount=74  
		simulationTime=300 ### ここいじる
	elif [ $traceFile == "mobility37_185.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=37
		simulationTime=100
    elif [ $traceFile == "mobility74_185.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=74
		simulationTime=100
    elif [ $traceFile == "mobility112_185.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=112
		simulationTime=100
    elif [ $traceFile == "mobility150_185.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=150
		simulationTime=100
    elif [ $traceFile == "mobility185.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=185
		simulationTime=100
	fi
	for protocol in GPSR NPGPSR NDGPSR
	do
		mkdir -p ~/"Simulation/$traceFile/$protocol"
		mkdir -p ~/"dataTemp"
		while [ $i -le $r ]; do
		
		echo "-run $i  --RoutingProtocol=$protocol "
		./waf --run "nagano-sim-alt --protocolName=$protocol --traceFile=/home/hry-user/ns-allinone-3.26/ns-3.26/node/$fileName --nodeCount=$nodeCount --simTime=$simulationTime" 
		
		mv ~/dataTemp/data.txt ~/dataTemp/data$i.txt
		mv ~/dataTemp/data$i.txt ~/Simulation/$traceFile/$protocol

		# ラインに送信	
		send_line_notification2 "$protocol"

		i=$((i + 1))
		
		done
		
		i=1
		rm -rf ~/dataTemp

	done
		
	send_line_notification3 "$traceFile" $nodeCount $simulationTime

done

./naganomaketable $r

end_time=`date +%s` #unix時刻から現在の時刻までの秒数を取得

SS=`expr ${end_time} - ${start_time}` #シュミレーションにかかった時間を計算する　秒数
HH=`expr ${SS} / 3600` #時を計算
SS=`expr ${SS} % 3600`
MM=`expr ${SS} / 60` #分を計算
SS=`expr ${SS} % 60` #秒を計算

echo "シュミレーション時間${HH}:${MM}:${SS}" #シミュレーションにかかった時間を　時:分:秒で表示する


# LINE通知を送信
send_line_notification4 $HH $MM $SS

for traceFile in mobility112.tcl mobility37_185.tcl mobility74_185.tcl mobility112_185.tcl mobility150_185.tcl mobility185.tcl # ここいじる
do
    send_line_notification5 $traceFile
done


# 不正ノードあり----------------------------------------------------------------↓

git stash
TARGET_BRANCH="recent_include-liar"
git checkout $TARGET_BRANCH
git pull origin recent_include-liar
g++ -std=c++11 -o naganomaketable naganomaketable.cc
branch=$(git rev-parse --abbrev-ref HEAD)

rm -rf ~/dataTemp
rm -rf ~/Simulation_liar/* #dataファイルの下を削除する

send_line_notification7() { 
	traceFile=$1
    nodeCount=$2
    simulationTime=$3
	protocol=$4
	message="$traceFile,  ノード数：${nodeCount},  simTime : ${simulationTime},  protocol : ${protocol}   終了"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

send_line_notification8() {
    traceFile=$1
    file_path="/home/hry-user/Simulation_liar/$traceFile/avarage.txt"
    if [ -f "$file_path" ]; then
        message=$(<"$file_path")
    fi
    curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

send_line_notification9() {
	branch=$1
    message="現在のブランチ：${branch}"
    curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

start_time=`date +%s` 

i=1 #loop
r=250 #実験回数   # ここいじる
nodeCount=0
simlationTime=0
send_line_notification1
send_line_notification9 $branch
for traceFile in mobility112_simTime=250.tcl mobility112_simTime=300.tcl # mobility37.tcl mobility112.tcl mobility185.tcl  ### ここいじる
do
	if [ $traceFile == "mobility112_simTime=250.tcl" ]; then
        fileName="mobility112.tcl"
		nodeCount=74  
		simulationTime=250 ### ここいじる
	elif [ $traceFile == "mobility112_simTime=300.tcl" ]; then
        fileName="mobility185.tcl"
		nodeCount=74
		simulationTime=300
    
	fi
	for protocol in GPSR NGPSR NPGPSR NDGPSR
	do
		mkdir -p ~/"Simulation_liar/$traceFile/$protocol"
		mkdir -p ~/"dataTemp"
		while [ $i -le $r ]; do
		
		echo "-run $i  --RoutingProtocol=$protocol "
		./waf --run "nagano-sim-alt --protocolName=$protocol --traceFile=/home/hry-user/ns-allinone-3.26/ns-3.26/node/$fileName --nodeCount=$nodeCount --simTime=$simulationTime" 
		
		mv ~/dataTemp/data.txt ~/dataTemp/data$i.txt
		mv ~/dataTemp/data$i.txt ~/Simulation_liar/$traceFile/$protocol

		# ラインに送信	
		send_line_notification2 "$protocol"

		i=$((i + 1))
		
		done
		
		i=1
		rm -rf ~/dataTemp

	done
		
	send_line_notification3 "$traceFile" $nodeCount $simulationTime

done

./naganomaketable $r

end_time=`date +%s` #unix時刻から現在の時刻までの秒数を取得

SS=`expr ${end_time} - ${start_time}` #シュミレーションにかかった時間を計算する　秒数
HH=`expr ${SS} / 3600` #時を計算
SS=`expr ${SS} % 3600`
MM=`expr ${SS} / 60` #分を計算
SS=`expr ${SS} % 60` #秒を計算

echo "シュミレーション時間${HH}:${MM}:${SS}" #シミュレーションにかかった時間を　時:分:秒で表示する


# LINE通知を送信
send_line_notification4 $HH $MM $SS

for traceFile in mobility112_simTime=250.tcl mobility112_simTime=300.tcl # ここいじる
do
    send_line_notification8 $traceFile
done

# simTime=300 ------------------------------------------------------------------------------------------↓
# git stash
# TARGET_BRANCH="recent"
# git checkout $TARGET_BRANCH
# git pull origin recent
# g++ -std=c++11 -o naganomaketable2 naganomaketable2.cc
# branch=$(git rev-parse --abbrev-ref HEAD)

# rm -rf ~/dataTemp
# rm -rf ~/Simulation300/* #dataファイルの下を削除する

# send_line_notification6() {
#     traceFile=$1
#     file_path="/home/hry-user/Simulation300/$traceFile/avarage.txt"
#     if [ -f "$file_path" ]; then
#         message=$(<"$file_path")
#     fi
#     curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
# }


# start_time=`date +%s`

# i=1 #loop
# r=250 #実験回数   # ここいじる
# nodeCount=0
# simlationTime=0
# send_line_notification1
# send_line_notification9 $branch
# for traceFile in mobility112.tcl mobility37_185.tcl mobility74_185.tcl mobility112_185.tcl mobility150_185.tcl mobility185.tcl   # mobility37.tcl mobility112.tcl mobility185.tcl  ### ここいじる
# do
# 	if [ $traceFile == "mobility112.tcl" ]; then
#         fileName="mobility112.tcl"
# 		nodeCount=74  
# 		simulationTime=300 ### ここいじる
# 	elif [ $traceFile == "mobility37_185.tcl" ]; then
#         fileName="mobility185.tcl"
# 		nodeCount=37
# 		simulationTime=300
#     elif [ $traceFile == "mobility74_185.tcl" ]; then
#         fileName="mobility185.tcl"
# 		nodeCount=74
# 		simulationTime=300
#     elif [ $traceFile == "mobility112_185.tcl" ]; then
#         fileName="mobility185.tcl"
# 		nodeCount=112
# 		simulationTime=300
#     elif [ $traceFile == "mobility150_185.tcl" ]; then
#         fileName="mobility185.tcl"
# 		nodeCount=150
# 		simulationTime=300
#     elif [ $traceFile == "mobility185.tcl" ]; then
#         fileName="mobility185.tcl"
# 		nodeCount=185
# 		simulationTime=300
# 	fi
# 	for protocol in GPSR NGPSR NPGPSR NDGPSR
# 	do
# 		mkdir -p ~/"Simulation300/$traceFile/$protocol"
# 		mkdir -p ~/"dataTemp"
# 		while [ $i -le $r ]; do
		
# 		echo "-run $i  --RoutingProtocol=$protocol "
# 		./waf --run "nagano-sim-alt --protocolName=$protocol --traceFile=/home/hry-user/ns-allinone-3.26/ns-3.26/node/$fileName --nodeCount=$nodeCount --simTime=$simulationTime" 
		
# 		mv ~/dataTemp/data.txt ~/dataTemp/data$i.txt
# 		mv ~/dataTemp/data$i.txt ~/Simulation300/$traceFile/$protocol

# 		# ラインに送信	
# 		send_line_notification2 "$protocol"

# 		i=$((i + 1))
		
# 		done

# 		send_line_notification7 "$traceFile" $nodeCount $simulationTime $protocol
		
# 		i=1
# 		rm -rf ~/dataTemp

# 	done
		
# 	send_line_notification3 "$traceFile" $nodeCount $simulationTime

# done

# ./naganomaketable2 $r

# end_time=`date +%s` #unix時刻から現在の時刻までの秒数を取得

# SS=`expr ${end_time} - ${start_time}` #シュミレーションにかかった時間を計算する　秒数
# HH=`expr ${SS} / 3600` #時を計算
# SS=`expr ${SS} % 3600`
# MM=`expr ${SS} / 60` #分を計算
# SS=`expr ${SS} % 60` #秒を計算

# echo "シュミレーション時間${HH}:${MM}:${SS}" #シミュレーションにかかった時間を　時:分:秒で表示する


# # LINE通知を送信
# send_line_notification4 $HH $MM $SS

# for traceFile in mobility112.tcl mobility37_185.tcl mobility74_185.tcl mobility112_185.tcl mobility150_185.tcl mobility185.tcl # ここいじる
# do
#     send_line_notification6 $traceFile
# done


# シミュレーション結果がまとまったファイルをgitのリポジトリにpushするようにすると神になれる 



