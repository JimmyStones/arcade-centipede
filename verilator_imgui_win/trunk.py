import sys
import binascii
import math

def main(args):
   namearray=args[0].split('.')
   romfile = open(namearray[0]+"2.hi", "w")  
   with open(args[0], "rb") as f:
    byte = f.read(1)
    count=0
    while byte:
        # Do stuff with byte.
        byte = f.read(1)
        if (byte):
          romfile.write(byte)
        count=count+1

if __name__ == "__main__":
   main(sys.argv[1:])
