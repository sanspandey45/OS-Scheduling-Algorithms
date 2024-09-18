import java.util.*;
import java.io.*;

public class Project3 {

	public static void main(String[] args) {
		
		// If no input file given
		if (args.length != 1) {
			System.out.println("Please input only the txt file name.");
		}
		
		String fileName = args[0];
		
		
		// Queue for each info from each line
		List<String> jobsList = new LinkedList<>();
		List<Integer> jobStartTime = new LinkedList<>();
		List<Integer> jobDuration = new LinkedList<>();
		
		// Read line-by-line
		try (BufferedReader bufRead = new BufferedReader(new FileReader(fileName))) {
			String lineOfData;
			
			while ( (lineOfData = bufRead.readLine() ) != null) {
				String[] separatedLine = lineOfData.split("\t");
				
				String job = separatedLine[0]; // converting string to char and saving
				int startTime = Integer.parseInt(separatedLine[1]);
				int duration = Integer.parseInt(separatedLine[2]);
				
				jobsList.add(job);
				jobStartTime.add(startTime);
				jobDuration.add(duration);
			}
			
		} catch (IOException e) {
            System.err.println("File Read Error: " + e.getMessage());
            e.printStackTrace();
		}
		
		// Running actual algorithms after queues ready.
		// Passing copies so it doesn't affect original.
		FCFS( new LinkedList<>(jobsList), new LinkedList<>(jobDuration) );
		RR( new LinkedList<>(jobsList), new LinkedList<>(jobStartTime), new LinkedList<>(jobDuration) );
	}
	
	public static void FCFS(List<String> jobsList, List<Integer> jobDuration) {

		System.out.println("FCFS: \n");
		int numOfJobs = jobsList.size();
		for (String job : jobsList) {
			System.out.print(job + " ");
		}
		
		System.out.println();
		
		for(int i = 0; i < numOfJobs; i++) {		
			// Print "i" spaces before job name.
			
			int duration = jobDuration.get(i);
			
			for(int y = 0; y < duration; y++) {
				for(int x = 0; x < i; x++) {
					System.out.print("  ");
				}
				System.out.println("X");
			}
		}
		
		System.out.println("\n\n");
		
		
	}
	
	public static void RR(List<String> jobsList, List<Integer> jobStartTime, List<Integer> jobDuration) {
		
		System.out.println("Round Robin: \n");
		int numOfJobs = jobsList.size();
		for (String job : jobsList) {
			System.out.print(job + " ");
		}
		
		System.out.println();
		
		Queue<Integer> nextJob = new LinkedList<>(); // Storing as index of jobList rather than name.
		List<Integer> runsLeft = new LinkedList<>(jobDuration); // Used to determine when job is done.
		List<String> finishedJobs = new LinkedList<>(); // Stored here once its value within runsLeft = 0.
		int quantum = 1;
		
		int currTime = -1; // Will use this to compare with arrival time.
		int currJob = -1; // Using index of job rather than name.
		
		// Processing stops once finishedJobs.size() = total number of jobs.
		while (finishedJobs.size() != numOfJobs) {

			// First run current job if there is one.
			if(! nextJob.isEmpty() ) {
				currJob = nextJob.poll();
				
				// Simulates "running" current job by placing x under corresponding job.
				for(int x = 0; x < quantum; x++) {
					for(int y = 0; y < currJob; y++) {
						System.out.print("  ");
					}
					System.out.println("X");
				}
				
			}
			
			// If there was no job, then currJob = -1 at this point.
			
			// Done running, so check new arrivals for next time value before queuing this current job again.
			currTime++; // Will start at 0
			
			// Iterate jobStartTime to find and prioritize new arrivals.
			for (int i = 0; i < jobStartTime.size(); i++) {
				if (jobStartTime.get(i) == currTime) {
					nextJob.add(i); // This job arrives at this time, so queue its index since it corresponds to jobsList index.
				}
			}
			
			// If no current job was found and run, then not necessary to reduce its number of runs left.
			if (currJob != -1 ) {
				
			//Reduce number of runs left.
				int newVal = runsLeft.get(currJob) - quantum;
				runsLeft.set(currJob,  newVal);
				
				if (runsLeft.get(currJob) == 0) {
					finishedJobs.add(jobsList.get(currJob));
				}
			
				else {
					nextJob.add(currJob);			
				}
			}
			
			currJob = -1;
			
		}
	}
}
