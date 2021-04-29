
import sys
import binascii
import math

def main(args):
   namearray=args[0].split('.')
   romfile = open(namearray[0]+".bin", "wb")  

   with open(args[0],mode="rb") as f:
      lines=f.readlines()
      for line in lines:
        hexes=line.split(' ')
        for hex in hexes:
          print(hex)
          i=int(hex,16)
          print(i)
          romfile.write(chr(i&0xff))
          #romfile.write((i).to_bytes(8, byteorder='big', signed=False))

if __name__ == "__main__":
   main(sys.argv[1:])
