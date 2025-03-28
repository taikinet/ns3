#include "gpsr-ptable.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <algorithm>
#include <cmath>
#include <string>

NS_LOG_COMPONENT_DEFINE ("GpsrTable");


namespace ns3 {
namespace gpsr {

/*
   GPSR position table
 */

PositionTable::PositionTable ()
{
        m_txErrorCallback = MakeCallback (&PositionTable::ProcessTxError, this);
        m_entryLifeTime = Seconds (2); //FIXME ハロータイムに合わせてパラメータ化する

}

Time
PositionTable::GetEntryUpdateTime (Ipv4Address id)
{
        if (id == Ipv4Address::GetZero ())
        {
                return Time (Seconds (0));
        }
        std::map<Ipv4Address, Metrix>::iterator i = m_table.find (id);
        return i->second.time; //記録が行われた時間を返す
}

/**
 * \brief Adds entry in position table
 */
//TODO 仕上がり速度
void
PositionTable::AddEntry (Ipv4Address id, Vector position, uint32_t flag)
{
		std::map<Ipv4Address, Metrix >::iterator i = m_table.find (id);
		//shinato
		// if(flag == 1)
		// {	
		// 	if (i != m_table.end () || id.IsEqual (i->first))
		// 	{
		// 		m_table.erase (id);
				
		// 		Metrix metrix;
		// 		position.x=1800;//宛先ノード近い
		// 		position.y=2250;
		// 		metrix.position=position;
		// 		metrix.time=Simulator::Now ();
		// 		m_table.insert (std::make_pair (id, metrix));
		// 		return; //返さないとと後の処理ができない
		// 	}
		// 	//IDがテーブルにないとき、IDを追加
		// 	Metrix metrix;
		// 	position.x=1800;//宛先ノード近い
		// 	position.y=2250;
			
		// 	metrix.position=position;
		// 	metrix.time=Simulator::Now ();
		// 	m_table.insert (std::make_pair (id, metrix));
		// 	return;
		// }
		// else if(flag == 2)
		// {	
		// 	if (i != m_table.end () || id.IsEqual (i->first))
		// 	{
		// 		m_table.erase (id);
				
		// 		Metrix metrix;
		// 		position.x=2320;//宛先ノード近い
		// 		position.y=1900;
		// 		metrix.position=position;
		// 		metrix.time=Simulator::Now ();
		// 		m_table.insert (std::make_pair (id, metrix));
		// 		return; //返さないとと後の処理ができない
		// 	}
		// 	//IDがテーブルにないとき、IDを追加
		// 	Metrix metrix;
		// 	position.x=2320;//宛先ノード近い
		// 	position.y=1900;
		// 	metrix.position=position;
		// 	metrix.time=Simulator::Now ();
		// 	m_table.insert (std::make_pair (id, metrix));
		// 	return;
		// }
                // else if(flag == 3)
		// {	
		// 	if (i != m_table.end () || id.IsEqual (i->first))
		// 	{
		// 		m_table.erase (id);
				
		// 		Metrix metrix;
		// 		position.x=1920;//宛先ノード近い
		// 		position.y=2100;
		// 		metrix.position=position;
		// 		metrix.time=Simulator::Now ();
		// 		m_table.insert (std::make_pair (id, metrix));
		// 		return; //返さないとと後の処理ができない
		// 	}
		// 	//IDがテーブルにないとき、IDを追加
		// 	Metrix metrix;
		// 	position.x=1920;//宛先ノード近い
		// 	position.y=2100;
		// 	metrix.position=position;
		// 	metrix.time=Simulator::Now ();
		// 	m_table.insert (std::make_pair (id, metrix));
		// 	return;
		// }
		// else{
			//テーブルのID、テーブルの更新、位置と速度の情報の追加
			if (i != m_table.end () || id.IsEqual (i->first))
			{
				m_table.erase (id);
				Metrix metrix;
				metrix.position=position;
				metrix.time=Simulator::Now ();
				m_table.insert (std::make_pair (id, metrix));
				return; //返さないとと後の処理ができない
			}

			//IDがテーブルにないとき、IDを追加
			Metrix metrix;
			metrix.position=position;
			metrix.time=Simulator::Now ();
			m_table.insert (std::make_pair (id, metrix));
			return;
		// }
}
/**
 * \brief Deletes entry in position table
 */
void PositionTable::DeleteEntry (Ipv4Address id)
{
        m_table.erase (id);
}

/**
 * \brief Gets position from position table
 * \param id Ipv4Address to get position from
 * \return Position of that id or NULL if not known
 */

//対応するアドレスノードの位置情報を取得
//TODO 取得速度の情報を得ることを追加→さらに転送速度の情報を得ることを検討

Vector
PositionTable::GetPosition (Ipv4Address id)
{

        NodeList::Iterator listEnd = NodeList::End ();
        for (NodeList::Iterator i = NodeList::Begin (); i != listEnd; i++)
        {
                Ptr<Node> node = *i;
                if (node->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () == id)
                {
                        return node->GetObject<MobilityModel> ()->GetPosition ();
                }
        }
        return PositionTable::GetInvalidPosition ();

}

Vector
PositionTable::GetVelocity (Ipv4Address id)
{

        NodeList::Iterator listEnd = NodeList::End ();
        for (NodeList::Iterator i = NodeList::Begin (); i != listEnd; i++)
        {
                Ptr<Node> node = *i;
                if (node->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () == id)
                {
                        return node->GetObject<MobilityModel> ()->GetVelocity ();
                }
        }
        return PositionTable::GetInvalidVelocity ();

}


/**
 * \brief Checks if a node is a neighbour
 * \param id Ipv4Address of the node to check
 * \return True if the node is neighbour, false otherwise
 */
bool
PositionTable::isNeighbour (Ipv4Address id)
{
        Purge();
        std::map<Ipv4Address, Metrix >::iterator i = m_table.find (id);
        //近隣ノード
        if (i != m_table.end () || id.IsEqual (i->first))
        {
                return true;
        }
        //近隣ノードではない
        return false;
}


/**
 * \brief remove entries with expired lifetime
 */

//時間切れになったテーブルの情報を削除する
void
PositionTable::Purge ()
{

        if(m_table.empty ())
        {
                return;
        }

        std::list<Ipv4Address> toErase;

        std::map<Ipv4Address, Metrix>::iterator i = m_table.begin ();
        std::map<Ipv4Address, Metrix >::iterator listEnd = m_table.end ();

        for (; !(i == listEnd); i++)
        {

                if (m_entryLifeTime + GetEntryUpdateTime (i->first) <= Simulator::Now ())
                {
                        toErase.insert (toErase.begin (), i->first); //時間を超えたら削除リストに追加

                }
        }
        toErase.unique (); //独自性

        std::list<Ipv4Address>::iterator end = toErase.end ();

        for (std::list<Ipv4Address>::iterator it = toErase.begin (); it != end; ++it)
        {

                m_table.erase (*it); //テーブル内の対応するリストのアドレスIDを削除する

        }
}

/**
 * \brief clears all entries
 */

//テーブルリストのすべての値をクリア
void
PositionTable::Clear ()
{
        m_table.clear ();
}

/**
 * \brief Gets next hop according to GPSR protocol
 * \param position the position of the destination node
 * \param nodePos the position of the node that has the packet
 * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
 */

Ipv4Address
PositionTable::BestNeighbor (Vector position, Vector nodePos)
{
  Purge ();

  double initialDistance = CalculateDistance (nodePos, position);

  if (m_table.empty ())
    {
      NS_LOG_DEBUG ("BestNeighbor table is empty; Position: " << position);
      return Ipv4Address::GetZero ();
    }     //テーブルが空の場合（隣にノードがない場合）

  Ipv4Address bestFoundID = m_table.begin ()->first;
  double bestFoundDistance = CalculateDistance (m_table.begin ()->second.position, position);
  std::map<Ipv4Address, Metrix >::iterator i;
  for (i = m_table.begin (); !(i == m_table.end ()); i++)
    {
      if (bestFoundDistance > CalculateDistance (i->second.position, position))
        {
          bestFoundID = i->first;
          bestFoundDistance = CalculateDistance (i->second.position, position);
        }
    }

  if(initialDistance > bestFoundDistance){
  	NS_LOG_DEBUG ("BestFoound ID is  " <<bestFoundID );
    return bestFoundID;
}
  else
    return Ipv4Address::GetZero (); //so it enters Recovery-mode

}

/**
 * \brief Gets next hop according to GPSR recovery-mode protocol (right hand rule)
 * \param previousHop the position of the node that sent the packet to this node
 * \param nodePos the position of the destination node
 * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
 */

//右手のルールに従って隣接するノードを見つけ、隣接するノードがない場合はaddress.getzero()を返す

Ipv4Address
PositionTable::BestAngle (Vector previousHop, Vector nodePos)
{
        Purge ();

        if (m_table.empty ())
        {
                NS_LOG_DEBUG (" Recovery-mode but neighbours table empty; Position: " << nodePos);
                return Ipv4Address::GetZero ();
        } //if table is empty (no neighbours)

        NS_LOG_DEBUG (" Recovery-mode start bestangle " << nodePos);
        double tmpAngle;
        Ipv4Address bestFoundID = Ipv4Address::GetZero ();
        double bestFoundAngle = 360;
        std::map<Ipv4Address, Metrix >::iterator i;

        for (i = m_table.begin (); !(i == m_table.end ()); i++)
        {
                tmpAngle = GetAngle(nodePos, previousHop, i->second.position);
                if (bestFoundAngle > tmpAngle && tmpAngle != 0)
                {
                        bestFoundID = i->first;
                        bestFoundAngle = tmpAngle;
                }
        }
        if(bestFoundID == Ipv4Address::GetZero ()) //パケットを送信したのが唯一の隣人である場合のみ
        {
                bestFoundID = m_table.begin ()->first;
        }
        return bestFoundID;
}


//ベクトルCentrePos-RefposとベクトルCentrePos-nodeの間の角度を反時計回りに与えます。
double
PositionTable::GetAngle (Vector centrePos, Vector refPos, Vector node){
        double const PI = 4*atan(1);

        std::complex<double> A = std::complex<double>(centrePos.x,centrePos.y);
        std::complex<double> B = std::complex<double>(node.x,node.y);
        std::complex<double> C = std::complex<double>(refPos.x,refPos.y); //Change B with C if you want angles clockwise

        std::complex<double> AB; //reference edge
        std::complex<double> AC;
        std::complex<double> tmp;
        std::complex<double> tmpCplx;

        std::complex<double> Angle;

        AB = B - A;
        AB = (real(AB)/norm(AB)) + (std::complex<double>(0.0,1.0)*(imag(AB)/norm(AB)));

        AC = C - A;
        AC = (real(AC)/norm(AC)) + (std::complex<double>(0.0,1.0)*(imag(AC)/norm(AC)));

        tmp = log(AC/AB);
        tmpCplx = std::complex<double>(0.0,-1.0);
        Angle = tmp*tmpCplx;
        Angle *= (180/PI);
        if (real(Angle)<0)
                Angle = 360+real(Angle);

        return real(Angle);

}





/**
 * \ProcessTxError
 */
void PositionTable::ProcessTxError (WifiMacHeader const & hdr)
{
}



//FIXME ainda preciso disto agr que o LS ja n está aqui???????

/**
 * \brief Returns true if is in search for destionation
 */
bool PositionTable::IsInSearch (Ipv4Address id)
{
        return false;
}

bool PositionTable::HasPosition (Ipv4Address id)
{
        return true;
}


}   // gpsr
} // ns3