#!/bin/bash

# === Configuration ===
BROKER="localhost"
PORT=1883
CLIENT_ID="anudb_test_$(date +%s)"
TOPIC_REQUEST="anudb/request"

# === Function to send a request ===
send_request() {
  local command="$1"
  local payload_body="$2"
  local req_id="${CLIENT_ID}_$(date +%s)"

  echo "Sending $command request..."

  # Build full JSON payload manually
  json="{\"command\":\"$command\",$payload_body\"request_id\":\"$req_id\"}"

  temp_file="payload_${req_id}.json"
  echo "$json" > "$temp_file"

  mosquitto_pub -h "$BROKER" -p "$PORT" -t "$TOPIC_REQUEST" -f "$temp_file"
  rm -f "$temp_file"
}

# === Start of testing ===
echo "Testing AnuDB MQTT API..."

send_request "create_collection" '"collection_name":"users",'
sleep 1

send_request "create_document" '"collection_name":"users","document_id":"user123","content":{"name":"John Doe","email":"john@example.com"},'

# === Product array ===
products=(
'p1001|Professional DSLR Camera Model X2000|Electronics|1299.99|42|PhotoTech|High-res camera with advanced features for enthusiasts|{"sensor":"24.2MP CMOS","iso":"100-51200","shutterSpeed":"1/8000-30s"}|4.8|5'
'p1002|Ergonomic Office Chair|Furniture|249.95|120|ComfortPlus|Premium chair with lumbar support and adjustability|{"material":"Mesh + Leather","adjustableHeight":true}|4.5|15'
'p1003|Smart Fitness Watch Pro|Wearables|179.99|85|FitTech|Tracks heart rate, sleep, workouts|{"display":"AMOLED","battery":"14 days","waterproof":"5ATM"}|4.7|0'
'p1004|Stainless Steel Cookware Set|Kitchen|349.50|35|ChefElite|10-piece premium cookware set|{"pieces":10,"material":"18/10 Stainless Steel"}|4.9|20'
'p1005|Ultra HD 4K Smart TV 55-inch|Electronics|699.99|28|VisionTech|Crystal-clear display with smart OS|{"resolution":"3840x2160","hdr":true,"refreshRate":"120Hz"}|4.6|10'
'p1006|Wireless Noise-Cancelling Headphones|Audio|249.99|64|SoundWave|Noise cancelling, 30hr battery life|{"bluetooth":"5.2","batteryLife":"30h"}|4.7|5'
'p1007|Blender with Variable Speed|Appliances|189.95|53|BlendMaster|Perfect for smoothies and soups|{"power":"1200W","capacity":"2L"}|4.4|15'
'p1008|Portable External SSD 1TB|Computer Hardware|149.99|105|DataPro|Ultra-fast external SSD with USB-C|{"capacity":"1TB","readSpeed":"1050MB/s"}|4.8|0'
'p1009|Adjustable Dumbbell Set 5-50lbs|Fitness|299.95|22|PowerFit|Space-saving adjustable weights|{"weightRange":"5-50lbs","incrementSize":"2.5lbs"}|4.6|10'
'p1010|Robot Vacuum Cleaner|Home|399.99|47|CleanTech|Laser mapping, app control, self-emptying|{"suction":"2700Pa","batteryLife":"180 mins"}|4.5|15'
)

for p in "${products[@]}"; do
  IFS='|' read -r id title category price stock manufacturer description features rating discount <<< "$p"
  created_at=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

  payload='"collection_name":"users","document_id":"'"$id"'","content":{'
  payload+='"id":"'"$id"'","title":"'"$title"'","category":"'"$category"'","price":'"$price"','
  payload+='"currency":"USD","stock":'"$stock"',"manufacturer":"'"$manufacturer"'","description":"'"$description"'",'
  payload+='"specifications":'"$features"',"rating":'"$rating"',"discount_percent":'"$discount"','
  payload+='"created_at":"'"$created_at"'","active":true},'

  send_request "create_document" "$payload"
  sleep 0.3
done

# === Further testing ===
sleep 10
send_request "read_document" '"collection_name":"users","document_id":"p1010",'
send_request "read_document" '"collection_name":"users",'
send_request "read_document" '"collection_name":"users","limit":1,'
sleep 10
send_request "delete_document" '"collection_name":"users","document_id":"user123",'
sleep 10
send_request "read_document" '"collection_name":"users",'
sleep 10
send_request "create_index" '"collection_name":"users","field":"price",'
sleep 10
send_request "find_documents" '"collection_name":"users","query":{"$gt":{"price":179.9}},'
sleep 10
send_request "delete_index" '"collection_name":"users","field":"price",'
sleep 10
send_request "delete_collection" '"collection_name":"users",'
sleep 10
send_request "read_document" '"collection_name":"users",'

echo -e "\nTest completed. Subscribe manually to 'anudb/response/+' to see responses."

