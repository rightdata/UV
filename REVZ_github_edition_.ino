#include <ESP8266WiFi.h>  // Libraries for ESP8266/Blynk
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>  // Libraries for timers instead of delays
SimpleTimer timer;

unsigned long interval = 600000L; // set this value to the time between measurements 
unsigned int arraysize = 6; // set this to the number of measurements taken before averaged and transmitted
byte tranfail = 5;

const byte UVPin = A0;  // UV Sensor pin
float UVi = 0;  // UV Index valnue --> change to byte if less precision required

char auth[] = "b042d9...";  // Blynk auth token
const char WEBSITE[] = "api.pushingbox.com"; //pushingbox API server
const String devid = "v7030D..."; //device ID from Pushingbox
byte count = 0;
float meanUV = 0;
float sum = 0;
float dataArray[] = {0, 0, 0, 0, 0, 0};

//The Arduino Map function but for floats
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void getData() {  //get data and store each data point in array every 10mins, after 1hr - take the mean and send to transmitData function
  digitalWrite(0, HIGH); //turn off transmission LED indicator from transmitData function (active low)
  Serial.println("Get data point " + String(count));

  float uvLevel = analogRead(UVPin);
  float UVi = mapfloat(uvLevel, 318, 870, 0.01, 15.0); //Convert the voltage to a UV intensity level
  Serial.print("UVi:");
  Serial.println(UVi);
  dataArray[count] = UVi;

  count++;

  if (count == arraysize) {
    count = 0;

    byte num_valid = 0;
    Serial.print("Data array: ");
    for (int i = 0; i < arraysize; i++) {
      if (dataArray[i] >= 0 && dataArray[i] < 20) {
        num_valid++;
        Serial.print(dataArray[i]);
        Serial.print(" ");
        sum = sum + dataArray[i];

      }//end if
      else {
        // A data entry has been dropped
        Serial.print("Entry ");
        Serial.print(i);
        Serial.println(" is invalid."); // IF ALL DATA ON GOOGLE SHEET IS 'nan' or '0' IT IS LIKELY THAT THE SENSOR VALUES ARE OUT OF RANGE (E.G. <319 - INDICATING VERY LOW SCALE READINGS OR SENSOR DISCONNECTED/FAULTY)

      }//end else
    }//end for
    Serial.println(" ");
    Serial.println("sum: " + String(sum));
    meanUV = sum / num_valid; // arithmetic mean of array holding UV values
    sum = 0.00;
    memset(dataArray, 0, sizeof(dataArray));

    //Check that dataArray is clear
    Serial.print("Cleared array: ");
    for (int i = 0; i < arraysize; i++) {
      Serial.print(dataArray[i]);
      Serial.print(" ");
    }//end for
    Serial.println(" ");
    Serial.println("meanUV: " + String(meanUV));
    Serial.println("Data captured");
    transmitData();
  } //end if
}//end else

void setup()
{
  Serial.begin(9600);
  pinMode(0, OUTPUT); //Transmit LED indicator
  Serial.println("Initialising/Attempting Wi-Fi Connection...");
  Blynk.begin(auth, "...", "...");    // (auth token, SSID, Password) --> program does not run until Wi-Fi connection is established to Blynk server
  timer.setInterval(interval, getData);   // Set data collection update rate (10mins)
  Serial.println("Setup Complete");
}

void loop() {
  Blynk.run();
  timer.run();
}

void transmitData() {
  Serial.println("Attempting to transmit...");
  digitalWrite(0, LOW); //Turn on GPIO LED to indicate attempt to transmit data (active low)
  Blynk.virtualWrite(7, meanUV);

  WiFiClient client;  //Instantiate WiFi object for Google Sheets integration
  //Start or API service using our WiFi Client through PushingBox

  //This should be a while loop, keep trying until success
  byte infbreak = 0; //number of times loop has cycled (counter)
  byte number = 0; //declare variable to break from infinite loop once data is transmitted

  //The loop below cycles through until the PushingBox connection is made. If no connection is made after an arbritary amount (infbreak == some number),
  //the loop breaks and returns to the data collection process without transmission. The transmit LED will be lit during the attempts. 
  
  while (number == 0) { 

    while (client.connect(WEBSITE, 80))
    {
      client.print("GET /pushingbox?devid=" + devid + "&UVi="     + (String) meanUV); //transmit meanUV data
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(WEBSITE);
      client.println("User-Agent: ESP8266/1.0");
      client.println("Connection: close");
      client.println();
      Serial.println("Data Transmitted");
      infbreak = 0;
      number = 1; //break parent while loop 
      delay(200);
      break; //break nested while loop
    }

    infbreak++; //increment counter once every loop 
    Serial.println("Attempt: " + String(infbreak));
    
    if (infbreak == tranfail) { //set this to any value to increase/decrease time between retrying connection when transmission fails
      Serial.println("Continuing to next data collection sequence without transmission");
      infbreak == 0; //reset loop counter
      delay(200);
      break; //break parent while loop (could possibly be replaced with "number = 1") 
    }
  }
}



