
//No non temporal move expected

public class Main {                                                                                                                                                   

    final int iterations = 0x40000;

    public long checkSum(long[] tab, int n) {
        long s = 0;
        for (int i = 0; i < n ; i++) {
            s = s + tab[i];
        }
        return s;
    }


    public long testLoop(long[] tab) {
        long[] iterator = new long [iterations];
        for (int i = 0; i < iterations; i++) {
            if (i>-1) //in order to prevent non temporal move to simplify postprocessing
            iterator[i] = i;
        }
        for (long i : iterator) {
            tab[(int)i] = i;
        }
        return checkSum(tab, iterations);
    }

    public void test()
    {
        long[] tab = new long [iterations];
        System.out.println(testLoop(tab));
    }

    public static void main(String[] args)
    {
        new Main().test();
    }

}  

