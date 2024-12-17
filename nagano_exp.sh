LINE_NOTIFY_API="https://notify-api.line.me/api/notify"
TOKEN="uSmbrMW1BuCGlvovL2GlDPXbIoYqvOuMLVFRPcoUkUn"


rm -rf ~/dataTemp
rm -rf ~/Simulation/* #dataファイルの下を削除する
mkdir ~/"Simulation/GPSR"
mkdir ~/"Simulation/NGPSR"
mkdir ~/"Simulation/NPGPSR"
mkdir ~/"Simulation/NDGPSR" 

start_time=`date +%s` 

i=1 #loop
r=2 #実験回数

for protocol in GPSR NGPSR NPGPSR NDGPSR
do

	mkdir -p ~/"dataTemp"
	while [ $i -le $r ]; do
	
	echo "-run $i  --RoutingProtocol=$protocol "
	./waf --run "nagano-sim-alt --protocolName=$protocol "
	
	mv ~/dataTemp/data.txt ~/dataTemp/data$i.txt
	mv ~/dataTemp/data$i.txt ~/Simulation/$protocol

    # ラインに送信
	send_line_notification1() { 
		message="$protocol シミュレーション $i 回目終了"
		curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
	}	
	send_line_notification1

	i=`expr $i + 1`
	
	done
	
	i=1
	rm -rf ~/dataTemp

done

./naganomaketable $r

end_time=`date +%s` #unix時刻から現在の時刻までの秒数を取得

  SS=`expr ${end_time} - ${start_time}` #シュミレーションにかかった時間を計算する　秒数
  HH=`expr ${SS} / 3600` #時を計算
  SS=`expr ${SS} % 3600`
  MM=`expr ${SS} / 60` #分を計算
  SS=`expr ${SS} % 60` #秒を計算

  echo "シュミレーション時間${HH}:${MM}:${SS}" #シミュレーションにかかった時間を　時:分:秒で表示する

  send_line_notification2() {
    message="シミュレーションが終了しました。"
    curl -X POST -H "Authorization: Bearer $TOKEN" -F "message=$message" $LINE_NOTIFY_API
}

# LINE通知を送信
send_line_notification2
