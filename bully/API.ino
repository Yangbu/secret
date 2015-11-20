#include <XBee.h>
#include <math.h>
#include <SoftwareSerial.h>

// Define baud rate here
#define BAUD 9600
// Create an xBee object
SoftwareSerial xbeeSerial(2,3);
String identity = "";

// Looping Variables
boolean isLeader;
int leaderID;
int final_id;

boolean timeout_flag = false;
int timeout_count = 0;

int checkLeader_timer = 0;
int election_timer = 0;
int leader_timer = 0;

int election_timeout = 8;
int checkLeader_timeout = 8;
int leader_timeout = 5;

bool expireFlag = true; //new

uint8_t BEACON_ID = 1;
XBee xbee = XBee();


String getIdentity() {
  String s;

  // Enter configuration mode - Should return "OK" when successful.
  delay(1000);    // MUST BE 1000
  xbee.write("+++");
  delay(1000);    // MUST BE 1000
  xbee.write("\r");
  delay(100);

  // Get the OK and clear it out.
  while (xbee.available() > 0) {
    Serial.print(char(xbee.read()));
  }
  Serial.println("");

  // Send "ATNI" command to get the NI value of xBee.
  xbee.write("ATNI\r");
  delay(100);
  while (xbee.available() > 0) {
      s += char(xbee.read());
  }
  delay(100);

  // Exit configuration mode
  xbee.write("ATCN\r");
  delay(1000);

  // Flush Serial Buffer Before Start
  while (xbee.available() > 0) {
    Serial.print(char(xbee.read()));
  }
  Serial.println("");
  delay(100);

  return s;
}

void setup() {
  xbee.begin(BAUD);
  Serial.begin(BAUD);
  isLeader = false;
  identity = getIdentity();
  final_id = identity.toInt();
  leaderID = -1;
  Serial.println("My Identity is : "+ identity);
  Serial.println("Setup Complete");
}

// Read in the message
String readTheMsg() {
  String msg  = "";
  while(xbee.available() > 0) {
    char c = char(xbee.read());
    if (c == '\n') {
      break;
    }
    msg += c;
  }
  Serial.println(msg);
  return msg;
}

//rebroadcast leader id
void rebroadcastMsg(int id) {
  xbee.print(String(id) + ":Leader\n");
  Serial.println("Temp Leader :" + String(id));
}

void leaderBroadcast() {
  xbee.print(identity+ ":Alive\n");
  Serial.println("New Leader :" + String(leaderID));
}

boolean checkLeaderExpire() {
  if (checkLeader_timer >= checkLeader_timeout || leaderID == -1) {
    leaderID = -1;
    return true;
  } else {
    return false;
  }
}

boolean checkElectionTimeOut() {
  if (timeout_flag) {
    if (timeout_count < 3) {
      timeout_count++;
    } else {
      timeout_flag = false;
      timeout_count = 0;
    }
  }
  return timeout_flag;
}

void election(String info, int id) {
  Serial.println("Electing...");
  if (checkElectionTimeOut()) {
    return;
  }
  if (id > final_id) {
    final_id = id;
    election_timer = 0;
    rebroadcastMsg(final_id);
    Serial.println("here2");
  } else {
    if (election_timer >= election_timeout){
      election_timer = 0;
      timeout_count = 0;
      timeout_flag = true;
      leaderID = final_id;
      Serial.println("here4");
    } else {
      election_timer++;
      rebroadcastMsg(final_id);
      Serial.println("here3");
    }
  }
}
XBeeResponse response  = XBeeResponse();

//create reusable objects for responses we expect to handle

ZBRxResponse rx = ZBRxResponse();
ModemStatusResponse msr = ModemStatusResponse();

uint8_t command1[] = {'D','B'};  //command one
AtCommandRequest atRequest = AtCommandRequest(command1);

ZBTxStatusResponse txStatus = ZBTxStatusResponse();
AtCommandResponse atResponse = AtCommandResponse();

uint8_t payload[] = {5,6};
XBeeAddress64 broadcastAddr = XBeeAddress64(0x00000000, 0x0000FFFF); 
ZBTxRequest zbTx = ZBTxRequest(broadcastAddr, payload, sizeof(payload));

void processResponse(){
  if (xbee.getResponse().isAvailable()) {
      // got something
      Serial.println("xbee.getResponse is available");     
      if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
        // got a zb rx packet
        
        // now fill our zb rx class
        xbee.getResponse().getZBRxResponse(rx);
      
        Serial.println("Got an rx packet!");
            
        if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
            // the sender got an ACK
            Serial.println("packet acknowledged");
        } else {
          Serial.println("packet not acknowledged");
        }
        
        Serial.print("checksum is ");
        Serial.println(rx.getChecksum(), HEX);

        Serial.print("packet length is ");
        Serial.println(rx.getPacketLength(), DEC);
        
         for (int i = 0; i < rx.getDataLength(); i++) {
          Serial.print("payload [");
          Serial.print(i, DEC);
          Serial.print("] is ");
          Serial.println(rx.getData()[i], HEX);
        }
        
       for (int i = 0; i < xbee.getResponse().getFrameDataLength(); i++) {
        Serial.print("frame data [");
        Serial.print(i, DEC);
        Serial.print("] is ");
        Serial.println(xbee.getResponse().getFrameData()[i], HEX);
      }
        
      }
    } else if (xbee.getResponse().isError()) {
      Serial.print("error code:");
      Serial.println(xbee.getResponse().getErrorCode());
    }
}
void setup (){
  Serial.begin(9600);
  xbeeSerial.begin(9600);
  xbee.setSerial(xbeeSerial);
  Serial.println("Initializing transmitter...");
}

void sendTx(ZBTxRequest zbTx){
  xbee.send(zbTx);

   if (xbee.readPacket(5000)) {
    Serial.println("Got a Tx status response!");
    // got a response!

    // should be a znet tx status              
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);

      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        Serial.println("SUCCESS!");
      } else {
        Serial.println("FAILURE!");
        // the remote XBee did not receive our packet. is it powered on?
      }
    }
    else{
      Serial.println("ZB_TX_STATUS_RESPONSE false");
      Serial.println(txStatus.getDeliveryStatus());
    }
  } else if (xbee.getResponse().isError()) {
    Serial.print("Error reading packet.  Error code: ");  
    Serial.println(xbee.getResponse().getErrorCode());
  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    Serial.println("This should never happen...");
  }
}

void loop(){
  sendTx(zbTx);
  delay(1000);
  xbee.readPacket();
  processResponse();
 
  if (xbee.available()) {
    String msg = readTheMsg();
    String info = msg.substring(msg.indexOf(':') + 1);
    int id = msg.substring(0,msg.indexOf(':')).toInt();
    if (info == "Leader") {
      if(!expireFlag)
      election(info, id);
    } else if (info == "Alive"){
      expireFlag = false;  //new
      if (id == final_id) {
        leaderID = id;
        election_timer = 0;
        timeout_count = 0;  
        timeout_flag = true;
      }
      if (leaderID == id) {
        checkLeader_timer = 0;
        Serial.println("Leader ID : "+String(leaderID));
        
      } else {
        rebroadcastMsg(final_id);
//        final_id = 
        Serial.println("here5");
      }
    }
  } else {
    if (leaderID == identity.toInt()) {
      if (leader_timer == leader_timeout) {
        leader_timer = 0;
        leaderBroadcast();
      } else {
        leader_timer++;
      }
    }else if(checkLeader_timer==checkLeader_timeout){
        //fix the bug when remove the rest Arduino but leave one
        checkLeader_timer = 0;
        Serial.println("Leader ID : "+String(leaderID));
    }else {
      checkLeader_timer++;
      Serial.println("checkLeader_timer : " + String(checkLeader_timer) + "election_timer : " +  election_timer);
      if (checkLeaderExpire()) {
        if (election_timer < election_timeout) {
//          Serial.println("here6");
          rebroadcastMsg(final_id);
          election_timer++;
        } else {
          // election_timer = 0
          leaderID = final_id;
        }
      }
    }
  }
  delay(1000);
}
