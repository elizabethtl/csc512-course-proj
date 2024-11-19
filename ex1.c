int main(){
   int id;
   int n;
   scanf("%d, %d", &id, &n);
   int s = 0;
   for (int i=0;i<n;i++){
      s += rand();
   }
   printf("id=%d; sum=%d\n", id, n); 
}
