import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time

SOCKET = "/tmp/m4-test.sock"
CLIENT = Path("build-m4/aiws_send" ).resolve()

def receive_exact(connection, size):
    data =bytearray()
    while len(data) < size:
        chunk =connection.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Server closed before sending the full reply") #fixed this being on thesame line
        data.extend(chunk )
    return bytes( data )


def send_with_client(directory, filename, expected):
    result = subprocess.run(  [str(CLIENT), "--socket", SOCKET, "add", filename], cwd=directory, capture_output=True,text=True, timeout=8, )
    if result.returncode !=expected:
        raise RuntimeError(
            f"{filename!r}: expected {expected}, got {result.returncode}. "
            f"stderr: {result.stderr}" )
    if expected == 1 and not result.stderr.strip(): raise RuntimeError("Rejected request should explain the error on stderr")
    print(f"PASS: {filename!r} returned {expected}")


with tempfile.TemporaryDirectory() as temp:
    directory = Path(temp)
    unusual_name = "CR1 café with spaces\nsecond line.txt"
    (directory / unusual_name).write_text("CR-1 filename test\n", encoding="utf-8")
    send_with_client(   directory, unusual_name, 0)
    send_with_client(   directory, "missing.txt",1)

    split_file =directory / "CR1 split request.txt"
    split_file.write_text("CR-1 split request test\n", encoding="utf-8")
    payload =os.fsencode(split_file)
    frame =struct.pack("!I", len(payload)) + payload

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection: 
        connection.settimeout(5)
        connection.connect(SOCKET)
        # Split yurrr
        for part in ( frame[:2], frame[2:7], frame[7:] ): 
            connection.sendall(part)
            time.sleep(0.1  )

        size, =struct.unpack("!I", receive_exact(connection, 4))
        reply = receive_exact(connection, size)
        if reply != b"0":raise RuntimeError(f"Split request returned {reply!r}")

    print("PASS: split request accepted")
    input("Check that both test documents appeared in the GUI, then press Enter!: ")
