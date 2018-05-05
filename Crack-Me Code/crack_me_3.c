int main() {
   char * name = "Qwertyuiop"; // Must be length 10 to work b/c loop below
   int serialNumber = 0;
   int sideNumber = 526489; //Obtains from crack-me binary in OllyDBG
   
   for(int i = 0; i < 10; i++){
       serialNumber += (unsigned char) name[i];
       sideNumber += serialNumber;
   }
   
   int sum = serialNumber + sideNumber;
   int halfSide = sideNumber / 2;
   serialNumber = ((sum << 2) + (sum << 1) + sum) - serialNumber + (halfSide << 3) + (halfSide << 2) + halfSide;
   serialNumber *= ((sum << 2) + (sum << 1) + sum);

   if(serialNumber < 0){
       serialNumber -= serialNumber;
   }
   printf("%d", serialNumber);
}