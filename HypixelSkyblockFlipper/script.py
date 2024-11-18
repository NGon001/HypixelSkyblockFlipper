import sys
import base64
import zlib
import nbtlib

def process_data(data):
    try:
        # Base64 decoding and zlib decompression
        decoded_data = base64.b64decode(data)
        decompressed_data = zlib.decompress(decoded_data, zlib.MAX_WBITS | 16)
        
        # Save the decompressed data to a temporary NBT file
        with open("output.nbt", "wb") as f:
            f.write(decompressed_data)
        
        # Load the NBT data using nbtlib
        nbt_data = nbtlib.load("output.nbt")
        
        # Print the NBT data, ensuring it is encoded in UTF-8
        print(nbt_data, flush=True)

    except Exception as e:
        print(f"Error processing data: {e}", file=sys.stderr)

if __name__ == "__main__":
    # Get input from command-line argument
    input_data = sys.argv[1]
    process_data(input_data)
