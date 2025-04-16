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

# Products
$products = @(
    @{
        id="p1001"; title="Professional DSLR Camera Model X2000"; category="Electronics"; price=1299.99; stock=42; manufacturer="PhotoTech";
        description="High-res camera with advanced features for enthusiasts"; features='{"sensor": "24.2MP CMOS", "iso": "100-51200", "shutterSpeed": "1/8000-30s"}'; rating=4.8; discount=5
    },
    @{
        id="p1002"; title="Ergonomic Office Chair"; category="Furniture"; price=249.95; stock=120; manufacturer="ComfortPlus";
        description="Premium chair with lumbar support and adjustability"; features='{"material": "Mesh + Leather", "adjustableHeight": true}'; rating=4.5; discount=15
    },
    @{
        id="p1003"; title="Smart Fitness Watch Pro"; category="Wearables"; price=179.99; stock=85; manufacturer="FitTech";
        description="Tracks heart rate, sleep, workouts"; features='{"display": "AMOLED", "battery": "14 days", "waterproof": "5ATM"}'; rating=4.7; discount=0
    },
    @{
        id="p1004"; title="Stainless Steel Cookware Set"; category="Kitchen"; price=349.50; stock=35; manufacturer="ChefElite";
        description="10-piece premium cookware set"; features='{"pieces": 10, "material": "18/10 Stainless Steel"}'; rating=4.9; discount=20
    },
    @{
        id="p1005"; title="Ultra HD 4K Smart TV 55-inch"; category="Electronics"; price=699.99; stock=28; manufacturer="VisionTech";
        description="Crystal-clear display with smart OS"; features='{"resolution": "3840x2160", "hdr": true, "refreshRate": "120Hz"}'; rating=4.6; discount=10
    },
    @{
        id="p1006"; title="Wireless Noise-Cancelling Headphones"; category="Audio"; price=249.99; stock=64; manufacturer="SoundWave";
        description="Noise cancelling, 30hr battery life"; features='{"bluetooth": "5.2", "batteryLife": "30h"}'; rating=4.7; discount=5
    },
    @{
        id="p1007"; title="Blender with Variable Speed"; category="Appliances"; price=189.95; stock=53; manufacturer="BlendMaster";
        description="Perfect for smoothies and soups"; features='{"power": "1200W", "capacity": "2L"}'; rating=4.4; discount=15
    },
    @{
        id="p1008"; title="Portable External SSD 1TB"; category="Computer Hardware"; price=149.99; stock=105; manufacturer="DataPro";
        description="Ultra-fast external SSD with USB-C"; features='{"capacity": "1TB", "readSpeed": "1050MB/s"}'; rating=4.8; discount=0
    },
    @{
        id="p1009"; title="Adjustable Dumbbell Set 5-50lbs"; category="Fitness"; price=299.95; stock=22; manufacturer="PowerFit";
        description="Space-saving adjustable weights"; features='{"weightRange": "5-50lbs", "incrementSize": "2.5lbs"}'; rating=4.6; discount=10
    },
    @{
        id="p1010"; title="Robot Vacuum Cleaner"; category="Home"; price=399.99; stock=47; manufacturer="CleanTech";
        description="Laser mapping, app control, self-emptying"; features='{"suction": "2700Pa", "batteryLife": "180 mins"}'; rating=4.5; discount=15
    }
)

foreach ($p in $products) {
    $created_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    $payload = @"
"collection_name": "users",
"document_id": "$($p.id)",
"content": {
  "id": "$($p.id)",
  "title": "$($p.title)",
  "category": "$($p.category)",
  "price": $($p.price),
  "currency": "USD",
  "stock": $($p.stock),
  "manufacturer": "$($p.manufacturer)",
  "description": "$($p.description)",
  "specifications": $($p.features),
  "rating": $($p.rating),
  "discount_percent": $($p.discount),
  "created_at": "$created_at",
  "active": true
},
"@
    Send-Request -Command "create_document" -Payload $payload
    Start-Sleep -Milliseconds 300
}

sleep 1
Send-Request -Command "export_collection" -Payload '"collection_name": "users","dest_dir":"./product_mqtt_export/",'
sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users","document_id": "p1010",'
Send-Request -Command "read_document" -Payload '"collection_name": "users",'
Send-Request -Command "read_document" -Payload '"collection_name": "users","limit": 1,'
sleep 1
Send-Request -Command "delete_document" -Payload '"collection_name": "users","document_id": "user123",'
sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users",'
sleep 1
Send-Request -Command "create_index" -Payload '"collection_name": "users","field": "price",'
sleep 1
# Can also use different operators that ANuDB supports, such as $lt, $gt, $eq, $orderby, $and, $or
Send-Request -Command "find_documents" -Payload '"collection_name": "users","query": { "$gt": { "price": 179.9 } },'
sleep 1
Send-Request -Command "delete_index" -Payload '"collection_name": "users","field": "price",'
sleep 1
Send-Request -Command "delete_collection" -Payload '"collection_name": "users",'
sleep 1
Send-Request -Command "read_document" -Payload '"collection_name": "users",'

Write-Host "`nTest completed. Subscribe manually to 'anudb/response/+' to see responses."

