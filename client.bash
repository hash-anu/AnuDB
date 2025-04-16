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
LOAD_TEST_THREADS=12
LOAD_TEST_OPERATIONS=100

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
  
  if ! command -v jq &> /dev/null; then
    echo -e "${YELLOW}Warning: jq not found. JSON formatting will be basic.${NC}"
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
  # Use jq if available, otherwise fall back to basic formatting
  if command -v jq &> /dev/null; then
    echo "$json" | jq 2>/dev/null || echo "$json" | sed 's/{"/ {\n  "/g' | sed 's/,"/,\n  "/g' | sed 's/}/\n}/g'
  else
    echo "$json" | sed 's/{"/ {\n  "/g' | sed 's/,"/,\n  "/g' | sed 's/}/\n}/g'
  fi
}

# Function to send a request and wait for response(s)
send_request() {
  local command="$1"
  local payload_body="$2"
  local req_id="${CLIENT_ID}_$(date +%s%N)"
  local success=false
  local expect_multiple=false
  local quiet_mode="$3"

  # Check if we should expect multiple responses
  if [ "$command" = "read_document" ] && [[ "$payload_body" != *"document_id"* ]]; then
    expect_multiple=true
  elif [ "$command" = "find_documents" ]; then
    expect_multiple=true
  fi

  if [ "$quiet_mode" != "quiet" ]; then
    echo -e "${YELLOW}Sending ${command} request...${NC}"
  fi

  # Build full JSON payload - handle empty payload_body for get_collections
  if [ -z "$payload_body" ]; then
    json="{\"command\":\"$command\",\"request_id\":\"$req_id\"}"
  else
    json="{\"command\":\"$command\",$payload_body\"request_id\":\"$req_id\"}"
  fi

  # Create a temporary file for the payload
  temp_file=$(mktemp)
  echo "$json" > "$temp_file"

  # Print formatted JSON if not in quiet mode
  if [ "$quiet_mode" != "quiet" ]; then
    echo -e "${BLUE}Request payload:${NC}"
    format_json "$json"
    echo ""
  fi

  # Create response file
  response_file=$(mktemp)

  # Start a background subscriber with a more reliable approach for multiple responses
  if [ "$expect_multiple" = true ]; then
    # Set up a filter for this specific request_id to avoid capturing unrelated responses
    response_filter="$req_id"
    
    if [ "$quiet_mode" != "quiet" ]; then
      echo -e "${YELLOW}Expecting multiple responses. Collecting for ${RESPONSE_TIMEOUT} seconds...${NC}"
    fi
    
    # Start the subscriber with a filter for our request_id before sending the request
    # Use a fifo for more reliable data capture
    fifo_file=$(mktemp -u)
    mkfifo "$fifo_file"
    
    # Launch background process to read from fifo
    cat "$fifo_file" > "$response_file" &
    cat_pid=$!
    
    # Start mosquitto_sub and redirect to fifo
    (timeout $RESPONSE_TIMEOUT mosquitto_sub -h "$BROKER" -p "$PORT" -t "$TOPIC_RESPONSE" -v | grep -F "$req_id" > "$fifo_file") &
    sub_pid=$!
    
    # Small delay to ensure subscriber is ready
    sleep 0.2
    
    # Send the request
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "$TOPIC_REQUEST" -f "$temp_file"
    
    # Wait for the timeout to complete
    wait $sub_pid 2>/dev/null
    
    # Kill the cat process
    kill $cat_pid 2>/dev/null
    
    # Remove the fifo
    rm -f "$fifo_file"
    
    # Check if we got any responses
    if [ -s "$response_file" ]; then
      if [ "$quiet_mode" != "quiet" ]; then
        echo -e "${GREEN}Responses received:${NC}"
        # Display each response on a new line
        cat "$response_file" | while read -r line; do
          echo -e "${BLUE}---${NC}"
          response_msg=$(echo "$line" | awk '{$1=""; print $0}') # Remove topic part
          format_json "$response_msg"
          echo ""
        done
      fi
      success=true
    else
      if [ "$quiet_mode" != "quiet" ]; then
        echo -e "${RED}No responses received within timeout period.${NC}"
      fi
    fi
  else
    # For single response, use a more reliable approach
    (timeout $RESPONSE_TIMEOUT mosquitto_sub -h "$BROKER" -p "$PORT" -t "$TOPIC_RESPONSE" -v | grep -m 1 -F "$req_id" > "$response_file") &
    sub_pid=$!
    
    # Small delay to ensure subscriber is ready
    sleep 0.2
    
    # Send the request
    mosquitto_pub -h "$BROKER" -p "$PORT" -t "$TOPIC_REQUEST" -f "$temp_file"
    
    # Wait for the response or timeout
    wait $sub_pid 2>/dev/null
    
    # Check if we got a response
    if [ -s "$response_file" ]; then
      if [ "$quiet_mode" != "quiet" ]; then
        echo -e "${GREEN}Response received:${NC}"
        response_content=$(cat "$response_file" | awk '{$1=""; print $0}') # Remove topic part
        format_json "$response_content"
      fi
      success=true
    else
      if [ "$quiet_mode" != "quiet" ]; then
        echo -e "${RED}No response received within timeout period.${NC}"
      fi
    fi
  fi

  # Clean up temporary files
  rm -f "$temp_file" "$response_file"

  # Pause for readability
  if [ "$quiet_mode" != "quiet" ]; then
    echo ""
    if [ "$quiet_mode" != "no_pause" ]; then
      read -p "Press Enter to continue..."
    fi
  fi

  return $([ "$success" = true ] && echo 0 || echo 1)
}

# Function to execute arbitrary commands
execute_arbitrary_command() {
  echo -e "\n${BLUE}Execute arbitrary AnuDB command${NC}"
  echo -e "${YELLOW}Available commands: create_collection, delete_collection, create_document, read_document, update_document, delete_document,${NC}"
  echo -e "${YELLOW}                   create_index, delete_index, find_documents, get_collections, get_indexes, export_collection${NC}"

  # Get command from user
  read -p "Enter command: " cmd

  # Get JSON payload from user (if needed)
  read -p "Enter JSON payload (excluding command and request_id, or leave empty): " payload

  # Send the request
  if [ -z "$payload" ]; then
    # For commands without payload like get_collections
    send_request "$cmd" ""
  else
    # Ensure payload ends with a comma for correct JSON formatting
    if [ "${payload: -1}" != "," ]; then
      payload="$payload,"
    fi
    send_request "$cmd" "$payload"
  fi
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
    '|Stainless Steel Cookware Set|Kitchen|349.50|35|ChefElite|10-piece premium cookware set|{"pieces":10,"material":"18/10 Stainless Steel"}|4.9|20'
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

  # Step 3: List all collections
  echo -e "\n${BLUE}Step $((step++)): Listing all collections${NC}"
  send_request "get_collections" "" || return 1

  # Step 4: Export collection to Json file
  echo -e "\n${BLUE}Step $((step++)): Exporting collection to dest directory${NC}"
  send_request "export_collection" "\"collection_name\":\"${COLLECTION_NAME}\",\"dest_dir\":\".\\demo\","  || return 1

  # Step 5: Read a specific document
  echo -e "\n${BLUE}Step $((step++)): Reading a specific product (p1001)${NC}"
  send_request "read_document" "\"collection_name\":\"${COLLECTION_NAME}\",\"document_id\":\"p1001\"," || return 1

  # Step 6: List all documents
  echo -e "\n${BLUE}Step $((step++)): Listing all products${NC}"
  send_request "read_document" "\"collection_name\":\"${COLLECTION_NAME}\"," || return 1

  # Step 7.1: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'price' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"price\"," || return 1

  # Step 7.2: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'category' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"category\"," || return 1

  # Step 7.3: Create an index
  echo -e "\n${BLUE}Step $((step++)): Creating an index on 'rating' field${NC}"
  send_request "create_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"rating\"," || return 1

  # Step 8: Get all indexes for the collection
  echo -e "\n${BLUE}Step $((step++)): Getting all indexes for collection '${COLLECTION_NAME}'${NC}"
  send_request "get_indexes" "\"collection_name\":\"${COLLECTION_NAME}\"," || return 1

  # Step 8: Demonstrate query operators
  # Note: Using double quotes for operators to avoid bash variable substitution

  # Step 9.1: $eq operator - Find a specific product by category
  echo -e "\n${BLUE}Step $((step++)): Finding products in 'Electronics' category using \$eq operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$eq\":{\"category\":\"Electronics\"}}," || return 1

  # Step 9.2: $gt operator - Find products over $300
  echo -e "\n${BLUE}Step $((step++)): Finding products priced over \$300 using \$gt operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$gt\":{\"price\":300.0}}," || return 1

  # Step 9.3: $lt operator - Find products under $200
  echo -e "\n${BLUE}Step $((step++)): Finding products priced under \$200 using \$lt operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$lt\":{\"price\":200.0}}," || return 1

  # Step 9.4: $and operator - Find products in Electronics with rating > 4.5
  echo -e "\n${BLUE}Step $((step++)): Finding Electronics products with rating > 4.5 using \$and operator${NC}"
  query="{\"\$and\":[{\"\$eq\":{\"category\":\"Electronics\"}},{\"\$gt\":{\"rating\":4.5}}]}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":$query," || return 1

  # Step 8.5: $or operator - Find products that are either Electronics or cost over $300
  echo -e "\n${BLUE}Step $((step++)): Finding products that are either Electronics or cost over \$300 using \$or operator${NC}"
  query="{\"\$or\":[{\"\$eq\":{\"category\":\"Electronics\"}},{\"\$gt\":{\"price\":300.0}}]}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":$query," || return 1

  # Step 9.6: $orderBy operator - List products ordered by price (descending)
  echo -e "\n${BLUE}Step $((step++)): Listing products ordered by price (descending) using \$orderBy operator${NC}"
  send_request "find_documents" "\"collection_name\":\"${COLLECTION_NAME}\",\"query\":{\"\$orderBy\":{\"price\":\"desc\"}}," || return 1

  # Step 10: Delete a document
  echo -e "\n${BLUE}Step $((step++)): Deleting a product (p1003)${NC}"
  send_request "delete_document" "\"collection_name\":\"${COLLECTION_NAME}\",\"document_id\":\"p1003\"," || return 1

  # Step 10.1: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the rating index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"rating\"," || return 1
  # Step 10.2: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the category index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"category\"," || return 1
  # Step 10.3: Delete the index
  echo -e "\n${BLUE}Step $((step++)): Deleting the price index${NC}"
  send_request "delete_index" "\"collection_name\":\"${COLLECTION_NAME}\",\"field\":\"price\"," || return 1

  # Step 11: Arbitrary command execution demo
  echo -e "\n${BLUE}Step $((step++)): Demonstrating arbitrary command execution${NC}"
  execute_arbitrary_command

  echo -e "\n${GREEN}Demo completed successfully!${NC}"
  return 0
}

# Generate a random document ID
generate_document_id() {
  echo "doc_$(date +%s%N | md5sum | head -c 10)"
}

# Function to perform load testing
# Function to perform load testing
perform_load_test() {
  local collection_name="${COLLECTION_NAME}_loadtest"
  local total_operations=$LOAD_TEST_OPERATIONS
  local num_threads=$LOAD_TEST_THREADS
  local operations_per_thread=$((total_operations / num_threads))
  
  echo -e "\n${BLUE}Starting load test with $num_threads threads, total $total_operations operations${NC}"
  echo -e "${YELLOW}Collection: $collection_name${NC}"
  
  # Create the test collection first
  echo -e "${YELLOW}Creating test collection...${NC}"
  send_request "create_collection" "\"collection_name\":\"${collection_name}\"," "no_pause" || {
    echo -e "${RED}Failed to create test collection. Aborting load test.${NC}"
    return 1
  }
  
  # Create some indexes
  echo -e "${YELLOW}Creating indexes...${NC}"
  send_request "create_index" "\"collection_name\":\"${collection_name}\",\"field\":\"price\"," "no_pause"
  send_request "create_index" "\"collection_name\":\"${collection_name}\",\"field\":\"category\"," "no_pause"
  
  # Function for the worker thread
  run_worker() {
    local thread_id=$1
    local operations=$2
    local collection=$3
    local start_time=$(date +%s.%N)
    local success_count=0
    local fail_count=0
    
    # Define available commands and their weights (probability)
    local commands=("create_document" "read_document" "update_document" "find_documents" "delete_document")
    local weights=(40 30 15 10 5)  # % probability
    
    # Create a temporary log file for this thread
    local log_file=$(mktemp)
    
    echo "Thread $thread_id starting $operations operations" >> "$log_file"
    
    for ((i=1; i<=operations; i++)); do
      # Select a random command based on weights
      local rand=$((RANDOM % 100))
      local cum=0
      local cmd_index=0
      for j in "${!weights[@]}"; do
        cum=$((cum + weights[j]))
        if [ "$rand" -lt "$cum" ]; then
          cmd_index=$j
          break
        fi
      done
      
      local command=${commands[$cmd_index]}
      local payload=""
      local doc_id=""
      
      # Generate appropriate payload for the command
      case $command in
        create_document)
          doc_id=$(generate_document_id)
          local price=$((RANDOM % 1000))
          local category=$(echo -e "Electronics\nFurniture\nKitchen\nClothing\nBooks" | shuf -n 1)
          payload="\"collection_name\":\"${collection}\",\"document_id\":\"${doc_id}\",\"content\":{\"id\":\"${doc_id}\",\"price\":${price},\"category\":\"${category}\",\"created_by\":\"thread-${thread_id}\"},"
          ;;
        read_document)
          if [ "$((RANDOM % 10))" -lt 7 ]; then  # 70% chance of reading a specific document
            # Try to read a document we just created, or a random one
            if [ -n "$doc_id" ]; then
              payload="\"collection_name\":\"${collection}\",\"document_id\":\"${doc_id}\","
            else
              payload="\"collection_name\":\"${collection}\",\"document_id\":\"$(generate_document_id)\","
            fi
          else
            # Read all documents
            payload="\"collection_name\":\"${collection}\","
          fi
          ;;
        update_document)
          if [ -n "$doc_id" ]; then
            local new_price=$((RANDOM % 1000))
            payload="\"collection_name\":\"${collection}\",\"document_id\":\"${doc_id}\",\"content\":{\"price\":${new_price},\"updated_by\":\"thread-${thread_id}\"},"
          else
            # Fall back to creating a document
            doc_id=$(generate_document_id)
            local price=$((RANDOM % 1000))
            local category=$(echo -e "Electronics\nFurniture\nKitchen\nClothing\nBooks" | shuf -n 1)
            payload="\"collection_name\":\"${collection}\",\"document_id\":\"${doc_id}\",\"content\":{\"id\":\"${doc_id}\",\"price\":${price},\"category\":\"${category}\",\"created_by\":\"thread-${thread_id}\"},"
            command="create_document"  # Override the command
          fi
          ;;
        find_documents)
          # Random query type
          local query_types=("eq" "gt" "lt" "and" "or" "orderBy")
          local query_type=${query_types[$((RANDOM % ${#query_types[@]}))]}
          
          case $query_type in
            eq)
              local category=$(echo -e "Electronics\nFurniture\nKitchen\nClothing\nBooks" | shuf -n 1)
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$eq\":{\"category\":\"${category}\"}},"
              ;;
            gt)
              local threshold=$((RANDOM % 500))
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$gt\":{\"price\":${threshold}}},"
              ;;
            lt)
              local threshold=$((500 + RANDOM % 500))
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$lt\":{\"price\":${threshold}}},"
              ;;
            and)
              local category=$(echo -e "Electronics\nFurniture\nKitchen\nClothing\nBooks" | shuf -n 1)
              local threshold=$((RANDOM % 800))
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$and\":[{\"\$eq\":{\"category\":\"${category}\"}},{\"\$gt\":{\"price\":${threshold}}}]},"
              ;;
            or)
              local category=$(echo -e "Electronics\nFurniture\nKitchen\nClothing\nBooks" | shuf -n 1)
              local threshold=$((RANDOM % 800))
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$or\":[{\"\$eq\":{\"category\":\"${category}\"}},{\"\$gt\":{\"price\":${threshold}}}]},"
              ;;
            orderBy)
              payload="\"collection_name\":\"${collection}\",\"query\":{\"\$orderBy\":{\"price\":\"desc\"}},"
              ;;
          esac
          ;;
        delete_document)
          if [ -n "$doc_id" ]; then
            payload="\"collection_name\":\"${collection}\",\"document_id\":\"${doc_id}\","
            doc_id=""  # Clear it so we don't try to use it again
          else
            # Fall back to reading all documents
            payload="\"collection_name\":\"${collection}\","
            command="read_document"  # Override the command
          fi
          ;;
      esac
      
      # Send the request and log the result
      if send_request "$command" "$payload" "quiet"; then
        success_count=$((success_count + 1))
      else
        fail_count=$((fail_count + 1))
      fi
      
      # Small random delay to simulate realistic load pattern (0-50ms)
      sleep 0.$(printf "%03d" $((RANDOM % 50)))
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local rate=$(echo "scale=2; $operations / $duration" | bc)
    
    echo "Thread $thread_id completed: $success_count successful, $fail_count failed, $rate ops/sec" >> "$log_file"
    
    # Return results
    echo "$thread_id|$success_count|$fail_count|$duration|$rate"
    
    # Clean up
    rm -f "$log_file"
  }
  
  # Create a temporary directory for thread output
  tmp_dir=$(mktemp -d)
  # Track PIDs to monitor them properly
  pids=()
  
  # Start the worker threads
  echo -e "${YELLOW}Starting worker threads...${NC}"
  
  # Start all worker threads in background
  for ((t=1; t<=num_threads; t++)); do
    run_worker "$t" "$operations_per_thread" "$collection_name" > "${tmp_dir}/thread_${t}.out" &
    pids+=($!)
    echo -e "  ${GREEN}Thread $t started (PID: ${pids[$((t-1))]})${NC}"
  done
  
  # Wait for all background jobs to complete with a progress indicator and timeout
  total_jobs=$num_threads
  max_wait_time=300  # 5 minutes max wait time
  start_time=$(date +%s)
  completed=0
  
  echo ""
  while [ $completed -lt $total_jobs ]; do
    completed=0
    still_running=()
    
    # Check each PID
    for pid in "${pids[@]}"; do
      if ps -p $pid > /dev/null; then
        still_running+=($pid)
      else
        completed=$((completed + 1))
      fi
    done
    
    # Update PIDs array to only contain running processes
    pids=("${still_running[@]}")
    
    # Calculate progress percentage
    percent=$((completed * 100 / total_jobs))
    
    # Check if we've been running too long
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    
    if [ $elapsed -gt $max_wait_time ]; then
      echo -e "\n${RED}Load test taking too long (${elapsed}s). Terminating remaining threads...${NC}"
      for pid in "${pids[@]}"; do
        kill -9 $pid 2>/dev/null
      done
      break
    fi
    
    # Display progress
    echo -ne "${YELLOW}Progress: ${GREEN}[$completed/$total_jobs] $percent% complete (${elapsed}s elapsed)${NC}\r"
    
    # If all done, break the loop
    if [ $completed -eq $total_jobs ]; then
      echo -e "${YELLOW}Progress: ${GREEN}[$total_jobs/$total_jobs] 100% complete (${elapsed}s elapsed)${NC}"
      break
    fi
    
    sleep 1
  done
  
  # If we still have running threads, they probably hung
  if [ ${#pids[@]} -gt 0 ]; then
    echo -e "\n${YELLOW}Some threads did not complete normally. Results may be incomplete.${NC}"
    for pid in "${pids[@]}"; do
      echo -e "${RED}Killing stuck thread PID $pid${NC}"
      kill -9 $pid 2>/dev/null
    done
  fi
  
  # Collect and display results
  echo -e "\n${BLUE}Load Test Results:${NC}"
  echo -e "${YELLOW}Thread | Success | Failed | Duration (s) | Rate (ops/sec)${NC}"
  echo -e "${BLUE}-----------------------------------------------------${NC}"
  
  total_success=0
  total_fail=0
  total_duration=0
  total_rate=0
  threads_reported=0
  
  for ((t=1; t<=num_threads; t++)); do
    if [ -f "${tmp_dir}/thread_${t}.out" ] && [ -s "${tmp_dir}/thread_${t}.out" ]; then
      IFS='|' read -r tid success fail duration rate < "${tmp_dir}/thread_${t}.out"
      total_success=$((total_success + success))
      total_fail=$((total_fail + fail))
      total_duration=$(echo "$total_duration + $duration" | bc)
      total_rate=$(echo "$total_rate + $rate" | bc)
      threads_reported=$((threads_reported + 1))
      
      printf "   %2d   |  %4d   |  %4d  |    %6.2f    |    %6.2f    \n" "$tid" "$success" "$fail" "$duration" "$rate"
    else
      printf "   %2d   |  %4s   |  %4s  |    %6s    |    %6s    \n" "$t" "N/A" "N/A" "N/A" "N/A"
    fi
  done
  
  echo -e "${BLUE}-----------------------------------------------------${NC}"
  
  if [ $threads_reported -eq 0 ]; then
    echo -e "${RED}No threads reported results! Test failed.${NC}"
  else
    avg_duration=$(echo "scale=2; $total_duration / $threads_reported" | bc)
    avg_rate=$(echo "scale=2; $total_rate / $threads_reported" | bc)
    total_operations=$((total_success + total_fail))
    
    printf "${GREEN}TOTAL   |  %4d   |  %4d  |    %6.2f    |    %6.2f    ${NC}\n" "$total_success" "$total_fail" "$avg_duration" "$avg_rate"
    echo -e "${BLUE}-----------------------------------------------------${NC}"
    echo -e "${GREEN}Total operations: $total_operations${NC}"
    echo -e "${GREEN}Success rate: $(echo "scale=2; $total_success * 100 / $total_operations" | bc)%${NC}"
    echo -e "${GREEN}Average throughput: $avg_rate operations/second${NC}"
    echo -e "${GREEN}Threads completed: $threads_reported/$num_threads${NC}"
  fi
  
  # Clean up the temporary directory
  rm -rf "$tmp_dir"
  
  # Ask if user wants to delete the test collection
  read -p "Do you want to delete the test collection? (y/N): " delete_collection
  if [[ "$delete_collection" == "y" || "$delete_collection" == "Y" ]]; then
    echo -e "${YELLOW}Deleting test collection...${NC}"
    send_request "delete_collection" "\"collection_name\":\"${collection_name}\"," "no_pause"
    echo -e "${GREEN}Test collection deleted.${NC}"
  else
    echo -e "${GREEN}Test collection '${collection_name}' has been preserved.${NC}"
  fi
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
  echo "  -x, --execute        Execute a single arbitrary command and exit"
  echo "  -l, --load-test      Run load test against the database"
  echo "  -t, --threads N      Number of threads for load testing (default: 12)"
  echo "  -n, --operations N   Number of operations for load testing (default: 100)"
  echo ""
}

# Parse command-line arguments
parse_args() {
  local execute_only=false
  local load_test_only=false

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
      -x|--execute)
        execute_only=true
        shift
        ;;
      -l|--load-test)
        load_test_only=true
        shift
        ;;
      -t|--threads)
        LOAD_TEST_THREADS="$2"
        shift 2
        ;;
      -n|--operations)
        LOAD_TEST_OPERATIONS="$2"
        shift 2
        ;;
      *)
        echo "Unknown option: $1"
        show_help
        exit 1
        ;;
    esac
  done

  # If execute_only flag is set, just run that function
  if [ "$execute_only" = true ]; then
    show_banner
    check_dependencies
    execute_arbitrary_command
    exit 0
  fi

  # If load_test_only flag is set, just run load testing
  if [ "$load_test_only" = true ]; then
    show_banner
    check_dependencies
    perform_load_test
    exit 0
  fi
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

  # Show menu
  echo -e "${BLUE}Select an option:${NC}"
  echo -e "  ${GREEN}1${NC}) Run demonstration"
  echo -e "  ${GREEN}2${NC}) Execute arbitrary command"
  echo -e "  ${GREEN}3${NC}) Run load test"
  echo -e "  ${GREEN}q${NC}) Quit"

  read -p "Choice: " choice

  case $choice in
    1)
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
      ;;
    2)
      execute_arbitrary_command
      ;;
    3)
      perform_load_test
      ;;
    q|Q)
      echo -e "${GREEN}Exiting.${NC}"
      exit 0
      ;;
    *)
      echo -e "${RED}Invalid choice.${NC}"
      exit 1
      ;;
  esac

  echo -e "\n${BLUE}Thank you for using the AnuDB MQTT API demonstration!${NC}"
}

# Run the main function
main "$@"
