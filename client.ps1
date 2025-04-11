# Configuration
$BROKER = "localhost"
$PORT = 1883
$CLIENT_ID = "anudb_test_$([int](Get-Date -UFormat %s))"
$TOPIC_REQUEST = "anudb/request"

# Function to send a request
function Send-Request {
    param (
        [string]$Command,
        [string]$Payload
    )
    $req_id = "${CLIENT_ID}_$([int](Get-Date -UFormat %s))"

    Write-Host "Sending $Command request..."

    $json = @"
{
  "command": "$Command",
  $Payload
  "request_id": "$req_id"
}
"@

    $tempFile = "payload_$req_id.json"
    $json | Set-Content -Encoding UTF8 $tempFile

    & mosquitto_pub.exe -h $BROKER -p $PORT -t $TOPIC_REQUEST -f $tempFile
    Remove-Item $tempFile -Force
}

# Create document helper
function Create-Product {
    param (
        [string]$product_id, [string]$title, [string]$category, [double]$price,
        [int]$stock, [string]$manufacturer, [string]$description,
        [string]$featuresJson, [double]$rating, [int]$discount
    )

    $req_id = "${CLIENT_ID}_product_${product_id}"
    $created_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

    Write-Host "Creating product ${product_id}: ${title}"

    $json = @"
{
  "command": "create_document",
  "collection_name": "users",
  "document_id": "$product_id",
  "content": {
    "id": "$product_id",
    "title": "$title",
    "category": "$category",
    "price": $price,
    "currency": "USD",
    "stock": $stock,
    "manufacturer": "$manufacturer",
    "description": "$description",
    "specifications": $featuresJson,
    "rating": $rating,
    "discount_percent": $discount,
    "created_at": "$created_at",
    "active": true
  },
  "request_id": "$req_id"
}
"@

    $tempFile = "payload_$req_id.json"
    $json | Set-Content -Encoding UTF8 $tempFile

    & mosquitto_pub.exe -h $BROKER -p $PORT -t $TOPIC_REQUEST -f $tempFile
    Remove-Item $tempFile -Force
    Start-Sleep -Milliseconds 300
}

# === Start of testing ===
Write-Host "Testing AnuDB MQTT API..."

Send-Request -Command "create_collection" -Payload '"collection_name": "users",'
sleep 1

Send-Request -Command "create_document" -Payload @'
"collection_name": "users",
"document_id": "user123",
"content": {
  "name": "John Doe",
  "email": "john@example.com"
},
'@

# Create 10 product documents
Create-Product "p1001" "Professional DSLR Camera Model X2000" "Electronics" 1299.99 42 "PhotoTech" `
"High-res camera with advanced features for enthusiasts" `
'{"sensor": "24.2MP CMOS", "iso": "100-51200", "shutterSpeed": "1/8000-30s"}' 4.8 5

Create-Product "p1002" "Ergonomic Office Chair" "Furniture" 249.95 120 "ComfortPlus" `
"Premium chair with lumbar support and adjustability" `
'{"material": "Mesh + Leather", "adjustableHeight": true}' 4.5 15

Create-Product "p1003" "Smart Fitness Watch Pro" "Wearables" 179.99 85 "FitTech" `
"Tracks heart rate, sleep, workouts" `
'{"display": "AMOLED", "battery": "14 days", "waterproof": "5ATM"}' 4.7 0

Create-Product "p1004" "Stainless Steel Cookware Set" "Kitchen" 349.50 35 "ChefElite" `
"10-piece premium cookware set" `
'{"pieces": 10, "material": "18/10 Stainless Steel"}' 4.9 20

Create-Product "p1005" "Ultra HD 4K Smart TV 55-inch" "Electronics" 699.99 28 "VisionTech" `
"Crystal-clear display with smart OS" `
'{"resolution": "3840x2160", "hdr": true, "refreshRate": "120Hz"}' 4.6 10

Create-Product "p1006" "Wireless Noise-Cancelling Headphones" "Audio" 249.99 64 "SoundWave" `
"Noise cancelling, 30hr battery life" `
'{"bluetooth": "5.2", "batteryLife": "30h"}' 4.7 5

Create-Product "p1007" "Blender with Variable Speed" "Appliances" 189.95 53 "BlendMaster" `
"Perfect for smoothies and soups" `
'{"power": "1200W", "capacity": "2L"}' 4.4 15

Create-Product "p1008" "Portable External SSD 1TB" "Computer Hardware" 149.99 105 "DataPro" `
"Ultra-fast external SSD with USB-C" `
'{"capacity": "1TB", "readSpeed": "1050MB/s"}' 4.8 0

Create-Product "p1009" "Adjustable Dumbbell Set 5-50lbs" "Fitness" 299.95 22 "PowerFit" `
"Space-saving adjustable weights" `
'{"weightRange": "5-50lbs", "incrementSize": "2.5lbs"}' 4.6 10

Create-Product "p1010" "Robot Vacuum Cleaner" "Home" 399.99 47 "CleanTech" `
"Laser mapping, app control, self-emptying" `
'{"suction": "2700Pa", "batteryLife": "180 mins"}' 4.5 15

sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users","document_id": "p1010",'
#sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users",'
#sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users","limit": 1,'
#sleep 1
Send-Request -Command "delete_document" -Payload '"collection_name": "users","document_id": "user123",'
sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users",'
#sleep 1
Send-Request -Command "delete_collection" -Payload '"collection_name": "users",'
Send-Request -Command "read_document" -Payload '"collection_name": "users",'
#
Write-Host "`nTest completed. Subscribe manually to 'anudb/response/+' to see responses."

