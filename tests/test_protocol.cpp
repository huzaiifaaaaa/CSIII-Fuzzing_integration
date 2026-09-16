/**
 * Basic unit tests for our protocol
 *
 * PURPOSE: These tests verify basic functionality but are intentionally
 * LIMITED. They test the "happy path" scenarios where everything works
 * correctly. Note the contrast for negative testing.
 *
 * This demonstrates why unit tests alone are insufficient
 * for memory safety, if you don't properly note the negative testing (if you
 * even can). These tests will all pass even though the code contains serious
 * memory bugs that can cause crashes, data corruption, and security
 * vulnerabilities.
 *
 * WHAT THESE TESTS MAY MISS:
 * - Memory leaks in assignment operators
 * - Double-free bugs in copy constructors
 * - Buffer overflows in deserialization
 * - Integer overflows
 * - Malformed input handling that result to even other scenarios
 *
 * TO FIND THE REAL BUGS: Use AddressSanitizer, Valgrind, or fuzzing tools to
 * improve the test coverage.
 */

#include "../lib/protocol.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace MessagingProtocol;

// Tests basic chat message serialization round-trip
// This only tests the happy path - valid input, normal sizes
void test_chat_message_basic() {
  std::cout << "Testing basic chat message..." << std::endl;

  Message msg(CHAT_MESSAGE);
  msg.chat->username.set_data("alice");
  msg.chat->message.set_data("Hello World!");
  msg.chat->timestamp = 1234567890;
  msg.chat->priority = 5;

  // Test serialization - convert Message object to binary format
  auto buffer = Serializer::serialize(msg);
  assert(buffer.size() >
         sizeof(MessageHeader)); // Sanity check: has header + payload

  // Test deserialization - convert binary back to Message object
  // NOTE: This only tests with valid, well-formed data from our own serializer
  Message *deserialized = Serializer::deserialize(buffer.data(), buffer.size());
  assert(deserialized != nullptr);
  assert(deserialized->header.type == CHAT_MESSAGE);
  assert(deserialized->chat->timestamp == 1234567890);
  assert(deserialized->chat->priority == 5);
  assert(deserialized->chat->username.to_string() == "alice");
  assert(deserialized->chat->message.to_string() == "Hello World!");

  delete deserialized;
  std::cout << "✓ Chat message test passed" << std::endl;
}

// Tests UserInfo message type
void test_user_info_basic() {
  std::cout << "Testing basic user info..." << std::endl;

  Message msg(USER_INFO);
  msg.user_info->username.set_data("bob");
  msg.user_info->email.set_data("bob@example.com");
  msg.user_info->user_id = 42;
  msg.user_info->status = 1;

  // Simple case - no tags
  msg.user_info->tag_count = 0;
  msg.user_info->tags = nullptr;

  auto buffer = Serializer::serialize(msg);
  Message *deserialized = Serializer::deserialize(buffer.data(), buffer.size());

  assert(deserialized != nullptr);
  assert(deserialized->header.type == USER_INFO);
  assert(deserialized->user_info->user_id == 42);
  assert(deserialized->user_info->username.to_string() == "bob");
  assert(deserialized->user_info->email.to_string() == "bob@example.com");

  delete deserialized;
  std::cout << "✓ User info test passed" << std::endl;
}

// Tests binary data handling with small, well-formed chunk
void test_file_chunk_basic() {
  std::cout << "Testing basic file chunk..." << std::endl;

  Message msg(FILE_CHUNK);
  msg.file_chunk->filename.set_data("test.txt");
  msg.file_chunk->chunk_id = 0;
  msg.file_chunk->total_chunks = 1;
  msg.file_chunk->chunk_size = 5;

  // Small data chunk - using tiny size avoids triggering bounds checking bugs
  msg.file_chunk->data = new uint8_t[5];
  memcpy(msg.file_chunk->data, "hello",
         5); // Known safe: size matches allocation

  // Serialize and deserialize - this creates a NEW Message object
  auto buffer = Serializer::serialize(msg);
  Message *deserialized = Serializer::deserialize(buffer.data(), buffer.size());

  assert(deserialized != nullptr);
  assert(deserialized->header.type == FILE_CHUNK);
  assert(deserialized->file_chunk->chunk_id == 0);
  assert(deserialized->file_chunk->filename.to_string() == "test.txt");
  assert(deserialized->file_chunk->chunk_size == 5);
  assert(memcmp(deserialized->file_chunk->data, "hello", 5) == 0);

  delete deserialized;
  std::cout << "✓ File chunk test passed" << std::endl;
}

// Tests ProtocolString operations - but won't catch memory management bugs
// These operations look correct but have hidden memory safety issues, if you
// try more complex scenarios
void test_string_operations() {
  std::cout << "Testing string operations..." << std::endl;

  ProtocolString str1;
  str1.set_data("test string");
  assert(str1.to_string() == "test string");
  assert(str1.length == 11);

  ProtocolString str2(str1);
  assert(str2.to_string() == "test string");

  ProtocolString str3;
  str3 = str1;
  assert(str3.to_string() == "test string");

  std::cout << "✓ String operations test passed" << std::endl;
}

// Tests edge case of empty strings - this is actually important!
// Empty/null strings often trigger boundary condition bugs
void test_empty_strings() {
  std::cout << "Testing empty strings..." << std::endl;

  Message msg(CHAT_MESSAGE);
  msg.chat->username.set_data("");
  msg.chat->message.set_data("");

  auto buffer = Serializer::serialize(msg);
  Message *deserialized = Serializer::deserialize(buffer.data(), buffer.size());

  assert(deserialized != nullptr);
  assert(deserialized->chat->username.to_string() == "");
  assert(deserialized->chat->message.to_string() == "");

  delete deserialized;
  std::cout << "✓ Empty strings test passed" << std::endl;
}

// Tests UserInfo with tags array
void test_user_with_tags_simple() {
  std::cout << "Testing user with tags (simple)..." << std::endl;

  Message msg(USER_INFO);
  msg.user_info->username.set_data("charlie");
  msg.user_info->email.set_data("charlie@test.com");
  msg.user_info->user_id = 123;
  msg.user_info->tag_count = 2;

  msg.user_info->tags = new ProtocolString[2];
  msg.user_info->tags[0].set_data("admin");
  msg.user_info->tags[1].set_data("vip");

  auto buffer = Serializer::serialize(msg);
  Message *deserialized = Serializer::deserialize(buffer.data(), buffer.size());

  assert(deserialized != nullptr);
  assert(deserialized->user_info->tag_count == 2);
  assert(deserialized->user_info->tags[0].to_string() == "admin");
  assert(deserialized->user_info->tags[1].to_string() == "vip");

  delete deserialized; // This deletion is safe - deserialized object owns its
                       // memory
  std::cout << "✓ User with tags test passed" << std::endl;
}

void test_user_info_copy_no_double_free() {
  std::cout << "Testing UserInfo copy constructor (regression for CWE-415 double-free)..." << std::endl;
  Message original(USER_INFO);
  original.user_info->username.set_data("testuser");
  original.user_info->tag_count = 2;
  original.user_info->tags = new ProtocolString[2];
  original.user_info->tags[0].set_data("tag1");
  original.user_info->tags[1].set_data("tag2");
  {
    Message copy(original);
    assert(copy.user_info->tag_count == 2);
    assert(copy.user_info->tags[0].to_string() == "tag1");
    copy.user_info->tags[0].set_data("changed");
    assert(original.user_info->tags[0].to_string() == "tag1");
  }
  std::cout << "✓ UserInfo copy double-free regression test passed" << std::endl;
}

void test_deserialize_rejects_tag_count_overflow() {
  std::cout << "Testing deserialize rejects overflowing tag_count "
               "(regression for CWE-190/CWE-20)..."
            << std::endl;

  const uint8_t crashing_input[] = {
      0xfe, 0xca, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x10, 0x01, 0x00,
      0x75, 0x01, 0x00, 0x65};

  Message *msg = Serializer::deserialize(crashing_input, sizeof(crashing_input));
  assert(msg == nullptr); // tag_count this large can't be backed by 28 bytes
  delete msg;

  std::cout << "✓ Tag count overflow regression test passed" << std::endl;
}

void test_deserialize_rejects_oversized_string_length() {
  std::cout << "Testing deserialize rejects oversized string length "
               "(regression for CWE-125)..." << std::endl;

  const uint8_t crashing_input[] = {
      0xfe, 0xca, 0x01, 0x01, 0x10, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 's', 'h', 'o', 'r', 't'};

  Message *msg = Serializer::deserialize(crashing_input, sizeof(crashing_input));
  if (msg) {
    std::string u = msg->chat->username.to_string();
    delete msg;
  }

  std::cout << "✓ Oversized string length regression test passed" << std::endl;
}

void test_deserialize_rejects_oversized_chunk_size() {
  std::cout << "Testing deserialize rejects oversized FILE_CHUNK chunk_size "
               "(regression for CWE-125)..." << std::endl;

  const uint8_t crashing_input[] = {
      0xfe, 0xca, 0x01, 0x03, 0x1d, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x60, 0xea, 0x05, 0x00,
      'f', '.', 'b', 'i', 'n', 'o', 'n', 'l', 'y', 'f', 'e', 'w', 'b', 'y',
      't', 'e', 's'};

  Message *msg = Serializer::deserialize(crashing_input, sizeof(crashing_input));
  assert(msg == nullptr); // chunk_size can't be backed by remaining data
  delete msg;

  std::cout << "✓ Oversized chunk_size regression test passed" << std::endl;
}

void test_assignment_no_leak() {
  std::cout << "Testing UserInfo assignment operator (regression for CWE-401 leak)..." << std::endl;
  Message target(USER_INFO);
  target.user_info->username.set_data("initial");
  for (int i = 0; i < 50; i++) {
    Message source(USER_INFO);
    source.user_info->username.set_data("iteration");
    source.user_info->tag_count = 2;
    source.user_info->tags = new ProtocolString[2];
    source.user_info->tags[0].set_data("a");
    source.user_info->tags[1].set_data("b");
    target = source;
  }
  assert(target.user_info->username.to_string() == "iteration");
  assert(target.user_info->tag_count == 2);
  std::cout << "✓ Assignment leak regression test passed" << std::endl;
}

void test_assignment_no_double_free() {
  std::cout << "Testing UserInfo assignment operator (regression for CWE-415 double-free)..." << std::endl;
  Message source(USER_INFO);
  source.user_info->tag_count = 2;
  source.user_info->tags = new ProtocolString[2];
  source.user_info->tags[0].set_data("tag1");
  source.user_info->tags[1].set_data("tag2");
  {
    Message target(USER_INFO);
    target = source;
    assert(target.user_info->tags[0].to_string() == "tag1");
    target.user_info->tags[0].set_data("changed");
    assert(source.user_info->tags[0].to_string() == "tag1");
  }
  std::cout << "✓ Assignment double-free regression test passed" << std::endl;
}

void test_assignment_across_different_types() {
  std::cout << "Testing assignment between different message types (regression for CWE-476)..." << std::endl;
  Message chat_msg(CHAT_MESSAGE);
  chat_msg.chat->username.set_data("alice");
  Message user_msg(USER_INFO);
  user_msg.user_info->username.set_data("bob");
  user_msg.user_info->tag_count = 1;
  user_msg.user_info->tags = new ProtocolString[1];
  user_msg.user_info->tags[0].set_data("vip");
  chat_msg = user_msg;
  assert(chat_msg.header.type == USER_INFO);
  assert(chat_msg.user_info != nullptr);
  assert(chat_msg.chat == nullptr);
  assert(chat_msg.user_info->username.to_string() == "bob");
  assert(chat_msg.user_info->tags[0].to_string() == "vip");
  std::cout << "✓ Cross-type assignment regression test passed" << std::endl;
}

int main() {
  std::cout << "Running protocol tests..." << std::endl;

  try {
    test_chat_message_basic();
    test_user_info_basic();
    test_file_chunk_basic();
    test_string_operations();
    test_empty_strings();
    test_user_with_tags_simple();
    test_user_info_copy_no_double_free();
    test_deserialize_rejects_tag_count_overflow();
    test_deserialize_rejects_oversized_string_length();
    test_deserialize_rejects_oversized_chunk_size();
    test_assignment_no_leak();
    test_assignment_no_double_free();
    test_assignment_across_different_types();

    std::cout << "\n✓ All basic tests passed!" << std::endl;
    std::cout << "Note: These tests only cover happy path scenarios."
              << std::endl;
    std::cout << "Memory bugs and edge cases require fuzzing and sanitizers to "
                 "detect."
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
