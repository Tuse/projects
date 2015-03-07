import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Scanner;

public class Parser {
	
	private final int QUERY_NUM_LINES = 1;
	private final int NUMSENTENCES_NUM_LINES = 1;
	
	private Scanner scanner;
	private String outputName;
	private KnowledgeBase KB;
	
	public Parser(String inputName, String outputName) throws FileNotFoundException {
		
		File inputFile = new File(inputName);
		scanner = new Scanner(inputFile);
		
		this.outputName = outputName;
		//setupOutputFile();
		
		try {
			readInputFile();
		}
		catch(IOException ioe) {
			System.out.println("I/O Exception while reading " + inputName);
		}
	}
	
	private void readInputFile() throws IOException {
		String unparsedQuery = readLines(QUERY_NUM_LINES, scanner).get(0); //query = one line
		Predicate query = readPredicate(unparsedQuery);
		ArrayList<Rule> KB_Rules = new ArrayList<Rule>();
		
		int numClauses = Integer.parseInt(readLines(NUMSENTENCES_NUM_LINES, scanner).get(0));
		
		ArrayList<String> unparsedPredicates = readLines(numClauses, scanner);
		
		for(String unparsedPredicate : unparsedPredicates) {
			Rule currRule = null;
			if(!hasLHS(unparsedPredicate)) {
				Predicate pred =  readPredicate(unparsedPredicate);
				currRule = new Rule(pred, null);
			}
			else {//separate each predicate into RH and LH sides
				String rhs = getRHS(unparsedPredicate);
				String lhs = getLHS(unparsedPredicate);
				ArrayList<String> lhsArr = separateLHS(lhs);
				
				ArrayList<Predicate> lhsPreds = new ArrayList<Predicate>();
				for(String lhsPred : lhsArr) {
					Predicate pred = readPredicate(lhsPred);
					lhsPreds.add(pred);
				}
				
				Predicate rhsPred =  readPredicate(rhs);
				
				currRule = new Rule(rhsPred, lhsPreds);
			}
			
			KB_Rules.add(currRule);
		}
		
		this.KB = new KnowledgeBase(query, KB_Rules);
	}
	
	//Check that the string has an implication operator
	private boolean hasLHS(String clause) {
		boolean hasLHS = true;
		int implicationIndex = clause.indexOf("=>");
		if(implicationIndex == -1) {
			hasLHS = false;
		}
		return hasLHS;
	}
	
	private String getLHS(String clause) {
		final String implicationOperator = "=>";
		
		int implicationIndex = clause.indexOf(implicationOperator);
		return clause.substring(0, implicationIndex);
	}
	
	private String getRHS(String clause) {
		final String implicationOperator = "=>";
		final int implicationOperatorLength = implicationOperator.length();
		
		int implicationIndex = clause.indexOf(implicationOperator);
		return clause.substring(implicationIndex+implicationOperatorLength, clause.length());
	}
	
	//Parse a string with multiple predicates into individual ones (only occurs on LHS)
	private ArrayList<String> separateLHS(String str) {
		final int DOESNT_EXIST = -1;
		
		ArrayList<String> preds = new ArrayList<String>();
		int currAmp = str.indexOf('&');
		
		while(currAmp != DOESNT_EXIST) {
			String newPred = str.substring(0, currAmp);
			preds.add(newPred);
			
			str = str.substring(currAmp+1, str.length());
			currAmp = str.indexOf('&');
		}
		preds.add(str);
		
		return preds;
	}
	
	//Parse an individual predicate into predicate, variables, and constants
	private Predicate readPredicate(String unparsedPred) {
		final int DOESNT_EXIST = -1;
		
		int openParIndex = unparsedPred.indexOf('(');
		int closeParIndex = unparsedPred.indexOf(')');
		String name = unparsedPred.substring(0, openParIndex);
		
		ArrayList<String> args = new ArrayList<String>();
		int commaIndex = unparsedPred.indexOf(',');
		if(commaIndex == DOESNT_EXIST) {
			String var = unparsedPred.substring(openParIndex+1, closeParIndex);
			args.add(var);
		}
		else {
			String var1 = unparsedPred.substring(openParIndex+1, commaIndex);
			String var2 = unparsedPred.substring(commaIndex+1, closeParIndex);
			args.add(var1);
			args.add(var2);
		}
		return new Predicate(name, args);
	}
	
	private ArrayList<String> readLines(int numLines, Scanner in) {
		int numLinesRead = 0;
		ArrayList<String> lines = new ArrayList<String>();
		
		while(in.hasNextLine()) {
			String nextLine = in.nextLine();
			numLinesRead++;
			lines.add(nextLine);
			if(numLinesRead == numLines) {
				break;
			}
		}

		return lines;
	}
	
	public void printOutput(String msg) {
		msg += "\n";
		
		File outputFile = new File(outputName);
		try {
			if(outputFile.exists()) {
				outputFile.delete();
			}
			outputFile.createNewFile();
			
			boolean append = true;
			BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile, append));
			//writer.flush();
			
			writer.write(msg);
			writer.close();
		}
		catch (IOException e) {
			System.out.println("I/O Error: output file");
			e.printStackTrace();
		}
	}
	
	public KnowledgeBase getKB() {
		return this.KB;
	}
}