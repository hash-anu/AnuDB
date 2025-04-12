#!/bin/bash

# ======================================================
# AnuDB MQTT API Production Demo
# ======================================================
# This script demonstrates the core functionality of AnuDB
# using the MQTT interface. It can be used for live demos
# or as a reference implementation.
# ======================================================

# ANSI color codes for better readability
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# === Configuration ===
BROKER="localhost"
PORT=1883
CLIENT_ID="anudb_demo_$(date +%s)"
TOPIC_REQUEST="anudb/request"
TOPIC_RESPONSE="anudb/response/+"
RESPONSE_TIMEOUT=5
COLLECTION_NAME="product_catalog"

# Function to display banner
show_banner() {
  clear
  echo -e "${BLUE}=================================================${NC}"
  echo -e "${BLUE}         AnuDB MQTT API Production Demo          ${NC}"
  echo -e "${BLUE}=================================================${NC}"
  echo -e "${YELLOW}Broker:${NC} $BROKER:$PORT"
  echo -e "${YELLOW}Client ID:${NC} $CLIENT_ID"
  echo -e "${YELLOW}Collection:${NC} $COLLECTION_NAME"
  echo -e "${BLUE}=================================================${NC}"
  echo ""
}

# Function to check dependencies
check_dependencies() {
  echo -e "${YELLOW}Checking dependencies...${NC}"

  local missing=false

  if ! command -v mosquitto_pub &> /dev/null; then
    echo -e "${RED}Error: mosquitto_pub not found. Please install the Mosquitto clients package.${NC}"
    missing=true
  fi

  if ! command -v mosquitto_sub &> /dev/null; then
    echo -e "${RED}Error: mosquitto_sub not found. Please install the Mosquitto clients package.${NC}"
    missing=true
  fi

  if $missing; then
    echo -e "${RED}Please install the missing dependencies and try again.${NC}"
    exit 1
  fi

  echo -e "${GREEN}All dependencies are installed.${NC}"
}

# Function to format JSON payload for display
format_json() {
  local json="$1"
  # Simple indentation for JSON display without jq dependency
  echo "$json" | sed 's/{"/ {\n  "/g' | sed 's/,"/,\n  "/g' | sed 's/}/\n}/g'
}

# Function to send a request and wait for response(s)
send_request() {
  local command="$1"
  local payload_body="$2"
  local req_id="${CLIENT_ID}_$(date +%s)"
  local success=false
  local expect_multiple=false
  
  # Check if we should expect multiple responses
  if [ "$command" = "read_document" ] && [[ "$payload_body" != *"document_id"* ]]; then
    expect_multiple=true
  elif [ "$command" = "find_documents" ]; then
    expect_multiple=true
  fi

  echo -e "${YELLOW}Sending ${command} request...${NC}"

  # Build full JSON payload
  json="{\"command\":\"$command\",$payload_body\"request_id\":\"$req_id\"}"

  # Create a temporary file for the payload
  temp_file=$(mktemp)
  echo "$json" > "$temp_file"

  # Print formatted JSON
  echo -e "${BLUE}Request payload:${NC}"
  format_json "$json"
  echo ""

  # Create response file
  response_file=$(mktemp)
  
  if [ "$expect_multiple" = true ]; then
    # Start a background process to listen for responses for a specified period
    echo -e "${YELLOW}Expecting multiple responses. Collecting for ${RESPONSE_TIMEOUT} seconds...${NC}"
    timeout $RESPONSE_TIMEOUT mosquitto_sub -h "$BROKER" -p "$PORT" -t "$TOPIC_RESPONSE" > "$response_file" &
    sub_pid=$!
    
    # Send the request
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "$TOPIC_REQUEST" -f "$temp_file"
    
    # Wait for the subscriber to complete
    wait $sub_pid 2>/dev/null
    
    # Check if we got any responses
    if [ -s "$response_file" ]; then
      echo -e "${GREEN}Responses received:${NC}"
      # Display each response on a new line
      cat "$response_file" | while read -r line; do
        echo -e "${BLUE}---${NC}"
        format_json "$line"
        echo ""
      done
      success=true
    else
      echo -e "${RED}No responses received within timeout period.${NC}"
    fi
  else
    # For single response, use the original approach
    mosquitto_sub -h "$BROKER" -p "$PORT" -t "$TOPIC_RESPONSE" -C 1 -W "$RESPONSE_TIMEOUT" > "$response_file" &
    sub_pid=$!

    # Send the request
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "$TOPIC_REQUEST" -f "$temp_file"

    # Wait for the response or timeout
    wait $sub_pid

    # Check if we got a response
    if [ -s "$response_file" ]; then
      echo -e "${GREEN}Response received:${NC}"
      response_content=$(cat "$response_file")
      format_json "$response_content"
      success=true
    else
      echo -e "${RED}No response received within timeout period.${NC}"
    fi
  fi

  # Clean up temporary files
  rm -f "$temp_file" "$response_file"

  # Pause for readability
  echo ""
  if [ "$3" != "no_pause" ]; then
    read -p "Press Enter to continue..."
  fi

  return $([ "$success" = true ] && echo 0 || echo 1)
}

# Function to demonstrate database operations
perform_demo() {
  local step=1

  # Step 1: Create the collection
  echo -e "\n${BLUE}Step $((step++)): Creating collection '${COLLECTION_NAME}'${NC}"
  send_request "create_collection" "\"collection_name\":\"${COLLECTION_NAME}\"," || return 1

  # Step 2: Add sample products
  echo -e "\n${BLUE}Step $((step++)): Adding sample products${NC}"

  # Sample product data
  products=(
    'p1001|Professional DSLR Camera Model X2000|Electronics|1299.99|42|PhotoTech|High-res camera with advanced features for enthusiasts|{"sensor":"24.2MP CMOS","iso":"100-51200","shutterSpeed":"1/8000-30s"}|4.8|5'
    'p1002|Ergonomic Office Chair|Furniture|249.95|120|ComfortPlus|Premium chair with lumbar support and adjustability|{"material":"Mesh + Leather","adjustableHeight":true}|4.5|15'
    'p1003|Smart Fitness Watch Pro|Wearables|179.99|85|FitTech|Tracks heart rate, sleep, workouts|{"display":"AMOLED","battery":"14 days","waterproof":"5ATM"}|4.7|0'
    'p1004|Stainless Steel Cookware Set|Kitchen|349.50|35|ChefElite|10-piece premium cookware set|{"pieces":10,"material":"18/10 Stainless Steel"}|4.9|20'
    'p1005|Ultra HD 4K Smart TV 55-inch|Electronics|699.99|28|VisionTech|Crystal-clear display with smart OS|{"resolution":"3840x2160","hdr":true,"refreshRate":"120Hz"}|4.6|10'
  )

  for p in "${products[@]}"; do
    IFS='|' read -r id title category price stock manufacturer description features rating discount <<< "$p"
    created_at=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    payload="\"collection_name\":\"${COLLECTION_NAME}\",\"document_id\":\"${id}\",\"content\":{"
    payload+="\"id\":\"${id}\",\"title\":\"${title}\",\"category\":\"${category}\",\"price\":${price},"
    payload+="\"currency\":\"USD\",\"stock\":${stock},\"manufacturer\":\"${manufacturer}\",\"description\":\"${description}\","
    payload+="\"specifications\":${features},\"rating\":${rating},\"discount_percent\":${discount},"
    payload+="\"created_at\":\"${created_at}\",\"active\":true},"

    send_request "create_document" "$payload" "no_pause" || return 1
    echo -e "${GREEN}Added product: ${title}${NC}"
  done

  # Step 3: Read a specific document
  echo -e "\n${BLUE}Step $((step++)): Reading a specific product (p1001)${NC}"
  send_request "read_document" "\"collection_name\":\"${COLLECTION_NAME}\",\"document_id\":\"p1001\"," || return 1

  # Step 4: List all documents
  echo -e "\n${BLUE}Step $((step++)): Listing all products${NC}"
  send_request "read_document" "\"collection_name\":\"${COLLECTION_NAME}\"," || return 1

  # Step 5.1: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'price' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"price\"," || return 1

  # Step 5.2: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'category' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"category\"," || return 1

  # Step 5.3: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'rating' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"rating\"," || return 1

  # Step 6: Demonstrate query operators
  # Note: Using double quotes for operators to avoid bash variable substitution

  # Step 6.1: $eq operator - Find a specific product by category
  echo -e "\n${BLUE}Step $((step++)): Finding products in 'Electronics' category using \$eq operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$eq\":{\"category\":\"Electronics\"}}," || return 1

  # Step 6.2: $gt operator - Find products over $300
  echo -e "\n${BLUE}Step $((step++)): Finding products priced over \$300 using \$gt operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$gt\":{\"price\":300.0}}," || return 1

  # Step 6.3: $lt operator - Find products under $200
  echo -e "\n${BLUE}Step $((step++)): Finding products priced under \$200 using \$lt operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$lt\":{\"price\":200.0}}," || return 1

  # Step 6.4: $and operator - Find products in Electronics with rating > 4.5
  echo -e "\n${BLUE}Step $((step++)): Finding Electronics products with rating > 4.5 using \$and operator${NC}"
  query="{\"\$and\":[{\"\$eq\":{\"category\":\"Electronics\"}},{\"\$gt\":{\"rating\":4.5}}]}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":$query," || return 1

  # Step 6.5: $or operator - Find products that are either Electronics or cost over $300
  echo -e "\n${BLUE}Step $((step++)): Finding products that are either Electronics or cost over \$300 using \$or operator${NC}"
  query="{\"\$or\":[{\"\$eq\":{\"category\":\"Electronics\"}},{\"\$gt\":{\"price\":300.0}}]}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":$query," || return 1

  # Step 6.6: $orderBy operator - List products ordered by price (descending)
  echo -e "\n${BLUE}Step $((step++)): Listing products ordered by price (descending) using \$orderBy operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$orderBy\":{\"price\":\"desc\"}}," || return 1

  # Step 7: Delete a document
  echo -e "\n${BLUE}Step $((step++)): Deleting a product (p1003)${NC}"
  send_request "delete_document" "\"collection_name\":\"${COLLECTION_NAME}\",\"document_id\":\"p1003\"," || return 1

  # Step 8.1: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the price index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"rating\"," || return 1
  # Step 8.2: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the price index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"category\"," || return 1
  # Step 8.3: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the price index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"price\"," || return 1

  echo -e "\n${GREEN}Demo completed successfully!${NC}"
  return 0
}

# Function for command-line options
show_help() {
  echo "Usage: $0 [options]"
  echo ""
  echo "Options:"
  echo "  -h, --help           Show this help message"
  echo "  -b, --broker HOST    Set MQTT broker address (default: localhost)"
  echo "  -p, --port PORT      Set MQTT broker port (default: 1883)"
  echo "  -c, --collection NAME Set collection name (default: product_catalog)"
  echo ""
}

# Parse command-line arguments
parse_args() {
  while [[ $# -gt 0 ]]; do
    case $1 in
      -h|--help)
        show_help
        exit 0
        ;;
      -b|--broker)
        BROKER="$2"
        shift 2
        ;;
      -p|--port)
        PORT="$2"
        shift 2
        ;;
      -c|--collection)
        COLLECTION_NAME="$2"
        shift 2
        ;;
      *)
        echo "Unknown option: $1"
        show_help
        exit 1
        ;;
    esac
  done
}

# Function to clean up (delete collection)
clean_up() {
  echo -e "\n${YELLOW}Cleaning up... Deleting collection '${COLLECTION_NAME}'${NC}"
  send_request "delete_collection" "\"collection_name\":\"${COLLECTION_NAME}\"," "no_pause"
  echo -e "\n${GREEN}Cleanup completed.${NC}"
}

# Main function
main() {
  parse_args "$@"
  show_banner
  check_dependencies

  # Ask for confirmation
  read -p "Press Enter to start the demo or Ctrl+C to exit..."

  # Trap Ctrl+C to clean up
  trap clean_up INT

  # Run the demo
  perform_demo

  # Ask if user wants to keep the collection
  echo ""
  read -p "Do you want to keep the collection '${COLLECTION_NAME}'? (y/n): " keep_collection
  if [[ "$keep_collection" != "y" && "$keep_collection" != "Y" ]]; then
    clean_up
  else
    echo -e "\n${GREEN}Collection '${COLLECTION_NAME}' has been preserved.${NC}"
    trap - EXIT
  fi

  echo -e "\n${BLUE}Thank you for watching the AnuDB MQTT API demonstration!${NC}"
}

# Run the main function
main "$@"
