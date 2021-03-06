//package OptimizationTests.TrivialLoopEvaluator.AndTests;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Comparator;

public class Main {

    public static final int maxIter=999;
    /**
     * @param args
     */
    public static void main(String[] args) {
        Main main = new Main();
        Class<Main> cls = Main.class;
        Method[] mets = cls.getDeclaredMethods();
        Arrays.sort(mets, new Comparator<Method>() {
            @Override
            public int compare(Method arg0, Method arg1) {
                return arg0.getName().compareTo(arg1.getName());
            }
        });
        for (Method met:mets){
            if (met.getName().equalsIgnoreCase("main"))
                continue;
            try {
                System.out.println("Test "+met.getName()+" result: "+met.invoke(main));
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (IllegalArgumentException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            }
        }
    }
    public int testInt1(){
        int i=0;
        int res=0;
        for (;i<5;i++){
            res&=i;
        }
        return res;
    }
    public int testInt2(){
        int i=0;
        int res=0;
        int tmp=-10;
        for (;i<50;i++){
            res&=i;
            tmp--;
            res&=tmp;
        }
        return res;
    }
    public int testInt3(){
        int i=0;
        int res=0;
        int tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=tmp;
            tmp++;
        }
        return res;
    }
    public int testInt4(){
        int i=0;
        int res=10000;
        int tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp;
        }
        return res;
    }
    public long testLong1(){
        long i=0;
        long res=0;
        for (;i<5;i++){
            res&=i;
        }
        return res;
    }
    public long testLong2(){
        long i=0;
        long res=0;
        long tmp=-10;
        for (;i<50;i++){
            res&=i;
            tmp--;
            res&=tmp;
        }
        return res;
    }
    public long testLong3(){
        long i=0;
        long res=0;
        long tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=tmp;
            tmp++;
        }
        return res;
    }
    public long testLong4(){
        long i=0;
        long res=10000;
        long tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp;
        }
        return res;
    }
    public byte testByte1(){
        byte i=0;
        byte res=0;
        for (;i<5;i++){
            res&=i;
        }
        return res;
    }
    public byte testByte2(){
        byte i=0;
        byte res=0;
        byte tmp=-10;
        for (;i<50;i++){
            res&=i;
            tmp--;
            res&=tmp;
        }
        return res;
    }
    public byte testByte3(){
        byte i=0;
        byte res=0;
        byte tmp=-10;
        for (i=Byte.MIN_VALUE;i<Byte.MAX_VALUE;i++){
            res&=tmp;
            tmp++;
        }
        return res;
    }
    public byte testByte4(){
        byte i=0;
        byte res=100;
        byte tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp;
        }
        return res;
    }
    public short testShort1(){
        short i=0;
        short res=0;
        for (;i<5;i++){
            res&=i;
        }
        return res;
    }
    public short testShort2(){
        short i=0;
        short res=0;
        short tmp=-10;
        for (;i<50;i++){
            res&=i;
            tmp--;
            res&=tmp;
        }
        return res;
    }
    public short testShort3(){
        short i=0;
        short res=0;
        short tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=tmp;
            tmp++;
        }
        return res;
    }
    public short testShort4(){
        short i=0;
        short res=10000;
        short tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp;
        }
        return res;
    }
    public int testInt5(){
        int i=0;
        int res=0;
        int tmp=0;
        for (;i<5;i++){
            res&=tmp&i;
        }
        return res;
    }
    public int testInt6(){
        int i=0;
        int res=0;
        int tmp=-10;
        for (;i<50;i++){
            res=i&tmp;
            tmp--;
            res&=tmp;
        }
        return res;
    }
    public int testInt7(){
        int i=0;
        int res=0;
        int tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=i&tmp;
            tmp++;
        }
        return res;
    }
    public int testInt8(){
        int i=0;
        int res=10000;
        int tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp&i;
        }
        return res;
    }
    public long testLong5(){
        long i=0;
        long res=0;
        long tmp=Long.MAX_VALUE;
        for (;i<5;i++){
            res&=i&tmp;
        }
        return res;
    }
    public long testLong6(){
        long i=0;
        long res=0;
        long tmp=-10;
        for (;i<50;i++){
            res&=i&tmp;
            tmp--;
        }
        return res;
    }
    public long testLong7(){
        long i=0;
        long res=0;
        long tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=tmp&i;
            tmp++;
        }
        return res;
    }
    public long testLong8(){
        long i=0;
        long res=10000;
        long tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp&i;
        }
        return res;
    }
    public byte testByte5(){
        byte i=0;
        byte res=0;
        byte tmp=Byte.MAX_VALUE;
        for (;i<5;i++){
            res&=i&tmp;
        }
        return res;
    }
    public byte testByte6(){
        byte i=0;
        byte res=0;
        byte tmp=-10;
        for (;i<50;i++){
            res&=i&tmp;
            tmp--;
        }
        return res;
    }
    public byte testByte7(){
        byte i=0;
        byte res=0;
        byte tmp=-10;
        for (i=Byte.MIN_VALUE;i<Byte.MAX_VALUE;i++){
            res&=tmp&i;
            tmp++;
        }
        return res;
    }
    public byte testByte8(){
        byte i=0;
        byte res=100;
        byte tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp&i;
        }
        return res;
    }
    public short testShort5(){
        short i=0;
        short res=0;
        short tmp=maxIter;
        for (;i<5;i++){
            res&=i&tmp;
        }
        return res;
    }
    public short testShort6(){
        short i=0;
        short res=0;
        short tmp=-10;
        for (;i<50;i++){
            res&=i&tmp;
            tmp--;
        }
        return res;
    }
    public short testShort7(){
        short i=0;
        short res=0;
        short tmp=-10;
        for (i=0;i<maxIter;i++){
            res&=tmp&i;
            tmp++;
        }
        return res;
    }
    public short testShort8(){
        short i=0;
        short res=10000;
        short tmp=-10;
        for (i=tmp;i<res;i++){
            res&=tmp&i;
        }
        return res;
    }
}
