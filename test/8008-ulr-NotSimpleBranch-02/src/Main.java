
public class Main {

    // Test not a simple loop: branches
    public int loop() {
    
        int sum = 0;
        for (int i = 0; i < 10000; i++) {
            sum += (i < 5000 ? i + 5 : i + 10);
        }
        return sum;
    }

    public static void main(String[] args) {
        int res = new Main().loop();
        System.out.println(res);
    }
}
