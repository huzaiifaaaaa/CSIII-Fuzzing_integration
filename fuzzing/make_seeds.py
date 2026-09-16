#!/usr/bin/env python3
import struct, os

MAGIC = 0xCAFE
VERSION = 1
CHAT_MESSAGE, USER_INFO, FILE_CHUNK = 1, 2, 3

def pstr(s: bytes) -> bytes:
    return struct.pack('<H', len(s)) + s

def header(mtype, payload_size, msg_id=1, magic=MAGIC, version=VERSION):
    return struct.pack('<HBBII', magic, version, mtype, payload_size, msg_id)

def chat_message(username=b"alice", message=b"hello", timestamp=1234567890, priority=3):
    payload = struct.pack('<QB', timestamp, priority) + pstr(username) + pstr(message)
    return header(CHAT_MESSAGE, len(payload)) + payload

def user_info(username=b"bob", email=b"bob@example.com", user_id=42, status=1, tags=None):
    tags = tags or []
    payload = struct.pack('<IHI', user_id, status, len(tags)) + pstr(username) + pstr(email)
    for t in tags:
        payload += pstr(t)
    return header(USER_INFO, len(payload)) + payload

def file_chunk(filename=b"a.bin", data=b"12345", chunk_id=0, total_chunks=1):
    payload = struct.pack('<IIH', chunk_id, total_chunks, len(data)) + pstr(filename) + data
    return header(FILE_CHUNK, len(payload)) + payload

def write(name, data, outdir):
    with open(os.path.join(outdir, name), 'wb') as f:
        f.write(data)

def main():
    deser_dir, round_dir = "corpus/deserialize", "corpus/roundtrip"
    os.makedirs(deser_dir, exist_ok=True)
    os.makedirs(round_dir, exist_ok=True)
    seeds = {}

    seeds["chat_basic.bin"] = chat_message()
    seeds["user_info_no_tags.bin"] = user_info()
    seeds["user_info_with_tags.bin"] = user_info(tags=[b"admin", b"premium", b"verified"])
    seeds["file_chunk_basic.bin"] = file_chunk()
    seeds["chat_empty_strings.bin"] = chat_message(username=b"", message=b"")
    seeds["user_info_empty_strings.bin"] = user_info(username=b"", email=b"", tags=[])
    seeds["file_chunk_zero_size.bin"] = file_chunk(data=b"")
    seeds["chat_long_message.bin"] = chat_message(message=b"A" * 2000)
    seeds["user_info_many_tags.bin"] = user_info(tags=[f"tag{i}".encode() for i in range(20)])
    seeds["bad_magic.bin"] = header(CHAT_MESSAGE, 0, magic=0xDEAD)
    seeds["bad_version.bin"] = header(CHAT_MESSAGE, 0, version=99)
    seeds["unknown_type.bin"] = header(0, 0)
    seeds["truncated_header.bin"] = struct.pack('<HBB', MAGIC, VERSION, CHAT_MESSAGE)
    seeds["header_only.bin"] = header(CHAT_MESSAGE, 100)

    # Probes for the specific planted bugs:
    bad_str_payload = struct.pack('<QB', 0, 0) + struct.pack('<H', 0xFFFF) + b"short"
    seeds["chat_oversized_string_len.bin"] = header(CHAT_MESSAGE, len(bad_str_payload)) + bad_str_payload

    huge_tagcount_payload = struct.pack('<IHI', 1, 0, 0x10000001) + pstr(b"u") + pstr(b"e")
    seeds["user_info_huge_tag_count.bin"] = header(USER_INFO, len(huge_tagcount_payload)) + huge_tagcount_payload

    oversized_chunk_payload = struct.pack('<IIH', 0, 1, 60000) + pstr(b"f.bin") + b"onlyfewbytes"
    seeds["file_chunk_oversized_size.bin"] = header(FILE_CHUNK, len(oversized_chunk_payload)) + oversized_chunk_payload

    for name, data in seeds.items():
        write(name, data, deser_dir)

    big = header(CHAT_MESSAGE, 1024 * 1024) + (b"X" * (1024 * 1024 + 100))
    write("zz_large_input.bin", big, deser_dir)

    rt = {
        "chat_like.bin": bytes([0, 5]) + b"alice" + bytes([5]) + b"hello" + bytes([0,0,0,1,3]),
        "user_like.bin": bytes([1, 3]) + b"bob" + bytes([16]) + b"bob@example.com" + bytes([0,0,0,42,1,2]) + bytes([4]) + b"tag1" + bytes([4]) + b"tag2",
        "file_like.bin": bytes([2, 5]) + b"a.bin" + bytes([0,0,0,0, 0,0,0,1]) + bytes(range(64)),
        "empty.bin": bytes([0, 0]),
    }
    for name, data in rt.items():
        write(name, data, round_dir)

    print(f"Wrote {len(seeds)+1} deserialize seeds, {len(rt)} roundtrip seeds")

if __name__ == "__main__":
    main()
