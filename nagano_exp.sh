LINE_NOTIFY_API="https://notify-api.line.me/api/notify"
TOKEN="uSmbrMW1BuCGlvovL2GlDPXbIoYqvOuMLVFRPcoUkUn"


rm -rf ~/dataTemp
rm -rf ~/Simulation/* #dataファイルの下を削除する


# ラインに送信
send_line_notification1() { 
	message="シミュレーション開始！\n↓--------------------------↓"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}
send_line_notification2() { 
	protocol=$1
	message="$protocol シミュレーション $i 回目終了"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}
send_line_notification3() { 
	traceFile=$1
	message="$traceFile :  終了"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

send_line_notification4() {
	message="シミュレーションが終了しました。"
	curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

start_time=`date +%s` 

i=1 #loop
r=5 #実験回数   # ここいじる
send_line_notification1
for traceFile in mobility37 mobility_tokai mobility74_112 mobility112 mobility37_185 mobility74_185 mobility112_185 mobility150_185 mobility185   # mobility37.tcl mobility112.tcl mobility185.tcl  ### ここいじる
do
	if [ $traceFile = "mobility37" ]; then
        fileName=mobility37.tcl
		nodeCount=37
		simulationTime=250     ### ここいじる
    elif [ $traceFile = "mobility_tokai" ]; then
        fileName=mobility_tokai.tcl
        nodeCount=37
        simulationTime=250
    elif [ $traceFile = "mobility74_112" ]; then
        fileName=mobility112.tcl
        nodeCount=74
        simulationTime=250
	elif [ $traceFile = "mobility112" ]; then
        fileName=mobility112.tcl
		nodeCount=112
		simulationTime=250
	elif [ $traceFile = "mobility37_185" ]; then
        fileName=mobility185.tcl
		nodeCount=37
		simulationTime=250
    elif [ $traceFile = "mobility74_185" ]; then
        fileName=mobility185.tcl
		nodeCount=74
		simulationTime=250
    elif [ $traceFile = "mobility112_185" ]; then
        fileName=mobility185.tcl
		nodeCount=112
		simulationTime=250
    elif [ $traceFile = "mobility150_185" ]; then
        fileName=mobility185.tcl
		nodeCount=150
		simulationTime=250
	elif [$traceFile = "mobility185"]; then
        fileName=mobility185.tcl
		nodeCount=185
		simlationTime=250
	fi
	for protocol in GPSR NGPSR NPGPSR NDGPSR
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
		
	send_line_notification3 "$traceFile"

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
send_line_notification4